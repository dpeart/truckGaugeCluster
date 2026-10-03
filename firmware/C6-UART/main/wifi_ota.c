#include "wifi_ota.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "mdns.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

#include "ota_handler.h"
#include "c6_uart.h"
#include "c6_modes.h"

static const char *TAG = "WIFI_OTA";

#define HOSTNAME "maingauge"
#define PROV_SSID "maingauge"
#define PROV_POP  "truck123"

static app_wifi_state_t s_wifi_state = APP_WIFI_STATE_INIT;
bool g_ota_mode_enabled = false;

static httpd_handle_t server = NULL;
static bool s_provisioning_active = false;

// OTA target: C6 (self) vs P4 (UART stream)
static ota_target_t s_ota_target = OTA_TARGET_C6;

// Mode to return to after provisioning completes
static c6_mode_t s_return_mode = MODE_TELEMETRY;

static void set_wifi_mode(app_wifi_state_t state);

// -----------------------------------------------------------------------------
// OTA target control
// -----------------------------------------------------------------------------
void wifi_ota_set_target(ota_target_t target)
{
    s_ota_target = target;

    const char *tstr = (target == OTA_TARGET_C6) ? "C6" : "P4";
    ESP_LOGI(TAG, "OTA target set to %s", tstr);
}

ota_target_t wifi_ota_get_target(void)
{
    return s_ota_target;
}

// -----------------------------------------------------------------------------
// mDNS setup
// -----------------------------------------------------------------------------
static void setup_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_STATE)
        {
            ESP_LOGW(TAG, "mDNS already initialized");
            return;
        }
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }

    mdns_hostname_set(HOSTNAME);
    mdns_instance_name_set(HOSTNAME);

    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "mDNS started with hostname '%s'", HOSTNAME);
}

// -----------------------------------------------------------------------------
// IP event handler: start mDNS once STA has IP
// -----------------------------------------------------------------------------
static void ip_event_handler(void *arg,
                             esp_event_base_t event_base,
                             int32_t event_id,
                             void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP received, starting mDNS");
        setup_mdns();
    }
}

// -----------------------------------------------------------------------------
// HTTP server + OTA handler (only for CONNECTED / C6/P4 OTA)
// -----------------------------------------------------------------------------
static void start_webserver(void)
{
    if (server == NULL)
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.lru_purge_enable = true;
        config.stack_size = 8192;

        if (httpd_start(&server, &config) == ESP_OK)
        {
            register_ota_handler(server);
            ESP_LOGI(TAG, "HTTP server started for OTA (target=%s)",
                     (s_ota_target == OTA_TARGET_C6) ? "C6" : "P4");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to start HTTP server");
        }
    }
}

static void stop_webserver(void)
{
    if (server)
    {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

// -----------------------------------------------------------------------------
// Wi-Fi provisioning event handler
// -----------------------------------------------------------------------------
static void wifi_prov_event_handler(void *arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void *event_data)
{
    if (event_base != WIFI_PROV_EVENT)
    {
        return;
    }

    switch (event_id)
    {
    case WIFI_PROV_START:
        ESP_LOGI(TAG, "Provisioning started (manager active=%d)", s_provisioning_active ? 1 : 0);
        s_provisioning_active = true;
        break;

    case WIFI_PROV_CRED_RECV:
    {
        wifi_sta_config_t *sta_cfg = (wifi_sta_config_t *)event_data;
        const char *ssid = (sta_cfg && sta_cfg->ssid[0] != '\0')
                               ? (const char *)sta_cfg->ssid
                               : "<empty>";
        ESP_LOGI(TAG, "Provisioning credentials received: SSID='%s'", ssid);
        break;
    }

    case WIFI_PROV_CRED_SUCCESS:
    {
        wifi_sta_config_t *sta_cfg = (wifi_sta_config_t *)event_data;

        const char *ssid = NULL;
        size_t ssid_len = 0;

        if (sta_cfg && sta_cfg->ssid[0] != '\0')
        {
            ssid = (const char *)sta_cfg->ssid;
            ssid_len = strlen(ssid);
            ESP_LOGI(TAG, "Provisioning SUCCESS, SSID='%s'", ssid);
        }
        else
        {
            ESP_LOGW(TAG, "Provisioning SUCCESS, but SSID is NULL/empty");
        }

        if (ssid && ssid_len > 0)
        {
            uart_send_frame(CMD_PROV_SUCCESS,
                            (const uint8_t *)ssid,
                            ssid_len);
        }
        else
        {
            uart_send_frame(CMD_PROV_SUCCESS, NULL, 0);
        }

        if (s_provisioning_active)
        {
            ESP_LOGI(TAG, "Deinitializing provisioning manager after SUCCESS");
            wifi_prov_mgr_deinit();
            s_provisioning_active = false;
        }

        stop_webserver();

        ESP_LOGI(TAG, "Switching to ESP-NOW-only mode after provisioning success");
        set_wifi_mode(APP_WIFI_STATE_ESP_NOW_ONLY);

        // Return to the mode that requested provisioning (OTA or whatever)
        ESP_LOGI(TAG, "Returning to previous mode after provisioning");
        current_mode = s_return_mode;
        break;
    }

    case WIFI_PROV_CRED_FAIL:
    {
        wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
        ESP_LOGW(TAG, "Provisioning FAILED, reason=%d", *reason);

        uart_send_frame(CMD_PROV_FAIL, NULL, 0);

        if (s_provisioning_active)
        {
            ESP_LOGI(TAG, "Deinitializing provisioning manager after FAIL");
            wifi_prov_mgr_deinit();
            s_provisioning_active = false;
        }

        stop_webserver();

        ESP_LOGI(TAG, "Falling back to ESP-NOW-only mode after provisioning failure");
        set_wifi_mode(APP_WIFI_STATE_ESP_NOW_ONLY);

        // On failure, always fall back to telemetry
        current_mode = MODE_TELEMETRY;
        break;
    }

    case WIFI_PROV_END:
        ESP_LOGI(TAG, "Provisioning session ended (manager active=%d)",
                 s_provisioning_active ? 1 : 0);
        break;

    default:
        break;
    }
}

// -----------------------------------------------------------------------------
// Wi-Fi setup
// -----------------------------------------------------------------------------
void network_setup(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_prov_event_handler,
                                               NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &ip_event_handler,
                                               NULL));

    ESP_LOGI(TAG, "Wi-Fi stack initialized");
}

// -----------------------------------------------------------------------------
// Wi-Fi state machine
// -----------------------------------------------------------------------------
static void set_wifi_mode(app_wifi_state_t state)
{
    if (s_wifi_state == state && state != APP_WIFI_STATE_PROVISIONING)
    {
        ESP_LOGI(TAG, "Wi-Fi state unchanged (%s), skipping reconfiguration",
                 wifi_state_to_str(state));
        return;
    }

    ESP_LOGI(TAG, "Wi-Fi state transition: %s -> %s",
             wifi_state_to_str(s_wifi_state),
             wifi_state_to_str(state));

    s_wifi_state = state;

    switch (state)
    {
    case APP_WIFI_STATE_INIT:
        ESP_LOGI(TAG, "Wi-Fi state: INIT");
        break;

    case APP_WIFI_STATE_ESP_NOW_ONLY:
        ESP_LOGI(TAG, "Wi-Fi state: ESP-NOW-only");

        // Always force STA mode + no power save
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

        // Start Wi-Fi if needed (safe to call even if already started)
        {
            esp_err_t err = esp_wifi_start();
            if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
            {
                ESP_LOGW(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
            }
        }

        // Setting channel can fail if Wi-Fi is mid-transition
        {
            esp_err_t err = esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
            if (err != ESP_OK)
            {
                ESP_LOGW(TAG, "esp_wifi_set_channel failed: %s", esp_err_to_name(err));
                // ESP-NOW will still work on the current channel
            }
        }

        // Clean up provisioning if needed
        if (s_provisioning_active)
        {
            ESP_LOGI(TAG, "Cleaning up provisioning manager in ESP-NOW-only mode");
            wifi_prov_mgr_deinit();
            s_provisioning_active = false;
        }

        // Ensure HTTP server is stopped
        stop_webserver();
        break;

    case APP_WIFI_STATE_PROVISIONING:
    {
        ESP_LOGI(TAG, "Wi-Fi state: PROVISIONING (SoftAP)");

        if (s_provisioning_active)
        {
            ESP_LOGI(TAG, "Provisioning already active, skipping re-init");
            break;
        }

        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_softap,
            .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE};

        ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1,
            PROV_POP,
            PROV_SSID,
            NULL));

        s_provisioning_active = true;
        ESP_LOGI(TAG, "Provisioning manager initialized (active=1)");
        // wifi_prov_mgr runs its own HTTP server
        break;
    }

    case APP_WIFI_STATE_CONNECTED:
        ESP_LOGI(TAG, "Wi-Fi state: CONNECTED (STA + HTTP server, OTA target=%s)",
                 (s_ota_target == OTA_TARGET_C6) ? "C6" : "P4");

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

        ESP_ERROR_CHECK(esp_wifi_connect());

        if (s_provisioning_active)
        {
            ESP_LOGI(TAG, "Cleaning up provisioning manager in CONNECTED mode");
            wifi_prov_mgr_deinit();
            s_provisioning_active = false;
        }

        start_webserver();
        break;
    }
}

void init_wifi_state_machine(void)
{
    ESP_LOGI(TAG, "Booting into ESP-NOW mode (provisioning only via P4/OTA command)");
    set_wifi_mode(APP_WIFI_STATE_ESP_NOW_ONLY);
}

// -----------------------------------------------------------------------------
// OTA mode control
// -----------------------------------------------------------------------------
void enter_ota_mode(void)
{
    g_ota_mode_enabled = true;

    // Check if provisioning exists before entering OTA
    wifi_config_t cfg = {0};
    esp_wifi_get_config(WIFI_IF_STA, &cfg);

    if (strlen((char *)cfg.sta.ssid) == 0)
    {
        ESP_LOGW(TAG, "OTA requested but provisioning missing. Entering provisioning mode.");

        // Remember the mode that requested OTA (C6_OTA or P4_OTA)
        s_return_mode = current_mode;

        // Switch to provisioning mode via mode loop
        current_mode = MODE_PROVISIONING;
        return;
    }

    const char *tstr = (s_ota_target == OTA_TARGET_C6) ? "C6 OTA" : "P4 OTA";
    ESP_LOGI(TAG, "Entering %s mode (from state=%s)",
             tstr, wifi_state_to_str(s_wifi_state));

    set_wifi_mode(APP_WIFI_STATE_CONNECTED);
}

void exit_ota_mode(void)
{
    g_ota_mode_enabled = false;

    ESP_LOGI(TAG, "Exiting OTA mode (from state=%s)", wifi_state_to_str(s_wifi_state));

    // Stop HTTP server
    stop_webserver();

    // Ensure Wi-Fi is not connecting or scanning
    esp_wifi_disconnect();
    esp_wifi_stop();

    // Force state machine to reconfigure Wi-Fi
    s_wifi_state = APP_WIFI_STATE_INIT;

    // Switch back to ESP-NOW mode
    set_wifi_mode(APP_WIFI_STATE_ESP_NOW_ONLY);
}

// -----------------------------------------------------------------------------
// Provisioning mode control
// -----------------------------------------------------------------------------
void enter_provisioning_mode(void)
{
    ESP_LOGI(TAG, "Entering provisioning mode (requested by P4/OTA, current state=%s)",
             wifi_state_to_str(s_wifi_state));
    set_wifi_mode(APP_WIFI_STATE_PROVISIONING);
}

void exit_provisioning_mode(void)
{
    ESP_LOGI(TAG, "Exiting provisioning mode (current state=%s)",
             wifi_state_to_str(s_wifi_state));

    if (s_provisioning_active)
    {
        ESP_LOGI(TAG, "Deinitializing provisioning manager on explicit exit");
        wifi_prov_mgr_deinit();
        s_provisioning_active = false;
    }

    stop_webserver();
    set_wifi_mode(APP_WIFI_STATE_ESP_NOW_ONLY);
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
app_wifi_state_t get_wifi_state(void)
{
    return s_wifi_state;
}

const char *wifi_state_to_str(app_wifi_state_t state)
{
    switch (state)
    {
    case APP_WIFI_STATE_INIT:
        return "INIT";
    case APP_WIFI_STATE_ESP_NOW_ONLY:
        return "ESP_NOW_ONLY";
    case APP_WIFI_STATE_PROVISIONING:
        return "PROVISIONING";
    case APP_WIFI_STATE_CONNECTED:
        return "CONNECTED";
    default:
        return "UNKNOWN";
    }
}

void get_wifi_ip_str(char *buf, size_t len)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif)
    {
        strncpy(buf, "N/A", len);
        return;
    }

    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(netif, &ip) == ESP_OK)
    {
        snprintf(buf, len, IPSTR, IP2STR(&ip.ip));
    }
    else
    {
        strncpy(buf, "N/A", len);
    }
}
