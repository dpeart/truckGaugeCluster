#include "wifi_ota.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mdns.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ota_handler.h"

static const char *TAG = "WIFI_OTA";

#define PROV_POP "abcd1234"
#define PROV_SSID "smallGauge5"
#define HOSTNAME "smallGauge5"

static httpd_handle_t server = NULL;
static esp_netif_t *sta_netif = NULL;
static app_wifi_state_t s_current_wifi_state = APP_WIFI_STATE_INIT;

static bool s_prov_mgr_initialized = false;
static bool s_prov_mgr_running = false;
bool g_ota_mode_enabled = false;

static void setup_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_STATE)
            return;
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set(HOSTNAME);
    mdns_instance_name_set(HOSTNAME);
}

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
            setup_mdns();
        }
    }
}

static void stop_webserver(void)
{
    if (server)
    {
        httpd_stop(server);
        server = NULL;
        mdns_free();
        ESP_LOGI(TAG, "HTTP Server and mDNS stopped");
    }
}

void set_wifi_mode(app_wifi_state_t new_state);

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            if (s_current_wifi_state == APP_WIFI_STATE_CONNECTED)
                esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            stop_webserver();
            if (s_current_wifi_state == APP_WIFI_STATE_CONNECTED)
                esp_wifi_connect();
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            if (s_current_wifi_state == APP_WIFI_STATE_CONNECTED)
                start_webserver();
        }
    }
    else if (event_base == WIFI_PROV_EVENT)
    {
        if (event_id == WIFI_PROV_CRED_SUCCESS)
        {
            ESP_LOGI(TAG, "Provisioning successful!");

            esp_restart();
        }
        else if (event_id == WIFI_PROV_CRED_FAIL)
        {
            ESP_LOGW(TAG, "Provisioning failed. Resetting provisioning state.");

            wifi_prov_mgr_stop_provisioning();
            wifi_prov_mgr_reset_provisioning();
            wifi_prov_mgr_deinit();
            s_prov_mgr_initialized = false;
            s_prov_mgr_running = false;

            set_wifi_mode(APP_WIFI_STATE_ESP_NOW_ONLY);
        }
    }
}

void set_wifi_mode(app_wifi_state_t new_state)
{
    // Removing the guard clause allows us to force a clean re-init
    // when moving from PROVISIONING to CONNECTED.
    s_current_wifi_state = new_state;

    esp_wifi_disconnect();
    esp_wifi_stop();

    switch (new_state)
    {
    case APP_WIFI_STATE_INIT:
        ESP_LOGI(TAG, "Wi-Fi: Initializing...");
        break;

    case APP_WIFI_STATE_ESP_NOW_ONLY:
        ESP_LOGI(TAG, "Wi-Fi: ESP-NOW telemetry mode");
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_ps(WIFI_PS_NONE);
        break;

    case APP_WIFI_STATE_PROVISIONING:
    {
        ESP_LOGI(TAG, "Wi-Fi: Provisioning mode (SoftAP)");
        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_softap,
            .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE};

        if (!s_prov_mgr_initialized)
        {
            ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
            s_prov_mgr_initialized = true;
        }

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1,
            PROV_POP,
            PROV_SSID,
            NULL));

        s_prov_mgr_running = true;
        break;
    }

    case APP_WIFI_STATE_CONNECTED:
        ESP_LOGI(TAG, "Wi-Fi: Connected (STA + OTA server)");
        // Explicitly set to STA mode to override any remaining AP state
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        esp_wifi_connect();
        esp_wifi_set_ps(WIFI_PS_NONE);
        start_webserver();
        break;

    default:
        ESP_LOGW(TAG, "Unhandled Wi-Fi state: %d", new_state);
        break;
    }
}

void network_setup(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(sta_netif, HOSTNAME);
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
}

void init_wifi_state_machine(void)
{
    set_wifi_mode(APP_WIFI_STATE_ESP_NOW_ONLY);
}

void enter_ota_mode(void)
{
    g_ota_mode_enabled = true;

    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);

    if (!provisioned)
    {
        ESP_LOGW(TAG, "OTA requested but provisioning missing. Entering provisioning mode.");
        set_wifi_mode(APP_WIFI_STATE_PROVISIONING);
        return;
    }

    // Already provisioned → go straight to OTA
    set_wifi_mode(APP_WIFI_STATE_CONNECTED);
}

void exit_ota_mode(void)
{
    g_ota_mode_enabled = false;

    if (s_prov_mgr_running)
    {
        ESP_LOGW(TAG, "OTA exit requested — stopping active provisioning service");
        wifi_prov_mgr_stop_provisioning();
        wifi_prov_mgr_reset_provisioning();
        wifi_prov_mgr_deinit();
        s_prov_mgr_initialized = false;
        s_prov_mgr_running = false;
    }

    set_wifi_mode(APP_WIFI_STATE_ESP_NOW_ONLY);
}

app_wifi_state_t get_wifi_state(void)
{
    return s_current_wifi_state;
}

const char *wifi_state_to_str(app_wifi_state_t state)
{
    const char *names[] = {"INIT", "ESP_NOW_ONLY", "PROVISIONING", "CONNECTED"};
    if (state >= 0 && state <= 3)
        return names[state];
    return "UNKNOWN";
}

void get_wifi_ip_str(char *buf, size_t len)
{
    esp_netif_ip_info_t ip_info;
    if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0)
    {
        snprintf(buf, len, IPSTR, IP2STR(&ip_info.ip));
    }
    else
    {
        snprintf(buf, len, "N/A");
    }
}
