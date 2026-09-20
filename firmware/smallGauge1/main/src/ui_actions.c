#include "esp_log.h"
#include "lvgl.h"
#include "screens.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "wifi_ota.h"

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

    case 2: // Factory Reset
        ESP_LOGI(TAG_UI, "Factory Reset button pressed");

        // 1. Erase NVS (WiFi creds, your settings, OTA flags, etc.)
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());

        // 2. Force bootloader to boot from factory partition
        const esp_partition_t *factory = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP,
            ESP_PARTITION_SUBTYPE_APP_FACTORY,
            NULL);

        if (factory)
        {
            ESP_LOGI(TAG_UI, "Setting boot partition to factory: %s", factory->label);
            ESP_ERROR_CHECK(esp_ota_set_boot_partition(factory));
        }
        else
        {
            ESP_LOGE(TAG_UI, "Factory partition not found!");
        }

        // 3. Reboot
        esp_restart();
        break;

    default:
        ESP_LOGW(TAG_UI, "Unknown button ID: %d", id);
        break;
    }
}
