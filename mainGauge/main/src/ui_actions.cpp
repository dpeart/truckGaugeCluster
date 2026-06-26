#include "ui_actions.h"
#include "esp_log.h"
#include "lvgl.h"
#include "actions.h"
#include "screens.h"
#include "p4_modes.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_err.h"

static const char *TAG_UI = "UI";

//
// ------------------------------------------------------------
// Screen switching (long press)
// ------------------------------------------------------------
//

void action_switch_screen(lv_event_t *e)
{
    ESP_LOGW(TAG_UI, "LONG PRESS EVENT FIRED!");

    int id = (int)lv_event_get_user_data(e);
    ESP_LOGI(TAG_UI, "User data = %d", id);

    // Leaving settings screen?
    if (id == 1) // going to MAIN screen
    {
        if ((current_mode == p4_mode_t::C6_OTA ||
             current_mode == p4_mode_t::P4_OTA))
        {
            if (!ota_upload_in_progress)
            {
                ESP_LOGW(TAG_UI, "Leaving settings — OTA idle, canceling");
                send_mode_telemetry();
            }
            else
            {
                ESP_LOGW(TAG_UI, "Leaving settings — OTA upload active, cancel blocked");
            }
        }
    }

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

//
// ------------------------------------------------------------
// Button actions
// ------------------------------------------------------------
//

void action_button_pressed(lv_event_t *e)
{
    int id = (int)lv_event_get_user_data(e);

    ESP_LOGI(TAG_UI, "Config button pressed, id=%d", id);

    switch (id)
    {
    //
    // -------------------------
    // P4 BUTTONS
    // -------------------------
    //
    case 0: // P4 OTA toggle
        ESP_LOGI(TAG_UI, "P4 OTA pressed");

        if (current_mode == p4_mode_t::P4_OTA)
        {
            if (!ota_upload_in_progress)
            {
                ESP_LOGW(TAG_UI, "Canceling P4 OTA, returning to telemetry");
                send_mode_telemetry();
                ota_upload_in_progress = false;
                current_mode = p4_mode_t::TELEMETRY;
            }
            else
            {
                ESP_LOGW(TAG_UI, "P4 OTA upload active — cancel blocked");
            }
        }
        else
        {
            send_mode_p4_ota();
        }
        break;

    case 1: // P4 Reboot
        ESP_LOGI(TAG_UI, "P4 Reboot pressed");
        esp_restart();
        break;

    case 2: // P4 Reset
    {
        ESP_LOGI(TAG_UI, "P4 Reset pressed");

        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());

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

        esp_restart();
        break;
    }

    //
    // -------------------------
    // C6 BUTTONS
    // -------------------------
    //
    case 10: // C6 OTA toggle
        ESP_LOGI(TAG_UI, "C6 OTA pressed");

        if (current_mode == p4_mode_t::C6_OTA)
        {
            if (!ota_upload_in_progress)
            {
                ESP_LOGW(TAG_UI, "Canceling C6 OTA, returning to telemetry");
                ota_upload_in_progress = false;
                send_mode_telemetry();
                current_mode = p4_mode_t::TELEMETRY;
            }
            else
            {
                ESP_LOGW(TAG_UI, "C6 OTA upload active — cancel blocked");
            }
        }
        else
        {
            send_mode_c6_ota();
        }
        break;

    case 11: // C6 Reboot
        ESP_LOGI(TAG_UI, "C6 Reboot pressed");
        send_mode_reboot();
        break;

    case 12: // C6 Reset
        ESP_LOGI(TAG_UI, "C6 Reset pressed");
        send_mode_factory_reset();
        break;

    default:
        ESP_LOGW(TAG_UI, "Unknown button ID: %d", id);
        break;
    }
}
