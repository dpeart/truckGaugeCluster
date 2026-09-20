#include "esp_log.h"
#include "lvgl.h"
#include "screens.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "wifi_ota.h"
#include "wifi_provisioning/manager.h"
#include "esp_wifi.h"

static const char *TAG_UI = "UI";

void action_switch_screen(lv_event_t *e)
{
    ESP_LOGW(TAG_UI, "LONG PRESS EVENT FIRED!");

    int id = (int)lv_event_get_user_data(e);
    ESP_LOGI(TAG_UI, "User data = %d", id);

    switch (id)
    {
    case 1:
        ESP_LOGI(TAG_UI, "Switching to MAIN screen");
        lv_scr_load(objects.main);
        break;

    case 0:
        ESP_LOGI(TAG_UI, "Switching to settings screen");
        lv_scr_load(objects.settings);
        break;
    }
}

void action_button_pressed(lv_event_t *e)
{
    int id = (int)lv_event_get_user_data(e);

    ESP_LOGI(TAG_UI, "Config button pressed, id=%d", id);

    switch (id)
    {

    case 0: // OTA toggle
        if (g_ota_mode_enabled)
        {
            ESP_LOGI(TAG_UI, "OTA button pressed: exiting OTA mode");
            exit_ota_mode();
        }
        else
        {
            ESP_LOGI(TAG_UI, "OTA button pressed: entering OTA mode");
            enter_ota_mode();
        }
        break;

    case 1: // Reboot
        ESP_LOGI(TAG_UI, "Reboot button pressed");
        esp_restart();
        break;

    case 2: // Reset Provisioning
        ESP_LOGI(TAG_UI, "Reset provisioning button pressed");

        // Remove Wi-Fi provisioning data only
        ESP_LOGI(TAG_UI, "Erasing Wi-Fi provisioning data...");
        wifi_prov_mgr_reset_provisioning();

        // Optional: also clear Wi-Fi driver STA config
        ESP_LOGI(TAG_UI, "Clearing Wi-Fi STA config...");
        wifi_config_t empty_cfg = {0};
        esp_wifi_set_config(WIFI_IF_STA, &empty_cfg);

        ESP_LOGI(TAG_UI, "Factory reset complete, rebooting...");
        esp_restart();
        break;

    default:
        ESP_LOGW(TAG_UI, "Unknown button ID: %d", id);
        break;
    }
}
