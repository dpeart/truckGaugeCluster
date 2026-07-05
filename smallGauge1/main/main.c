// #define DEBUG 1       // unused; esp-idf logging levels are configured via menuconfig
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"

#include "ota_handler.h"
#include "ST77916.h"  // LCD driver
#include "PCF85063.h" // RTC
#include "QMI8658.h"  // IMU
// #include "SD_MMC.h"   // (optional, init commented out)
#include "Wireless.h" // radio code
#include "TCA9554PWR.h"
#include "BAT_Driver.h"
#include "PWR_Key.h"

#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"
#include "lvgl.h"
#include "LVGL_Driver.h"

// UI + app headers
#include "src/ui/ui.h"
#include "src/updateUI.h"
#include "src/GaugePacket.h"
#include "src/espnow_receiver.h"
#include "src/lvgl_lock.h"
#include "src/wifi_ota.h"

#define CONFIG_ESPNOW_CHANNEL 1
#define LVGL_BUF_LEN (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT)

// These are the actual definitions
volatile bool ui_ready = false;
volatile bool lvgl_started = false;

static const char *TAG = "MAIN";
#define ONBOARD_LED_GPIO 2

static void lvgl_task(void *arg)
{
    static bool first = true;

    while (1)
    {
        lvgl_lock();

        if (first)
        {
            ESP_LOGI("LVGL", "Starting UI initialization...");
            ui_init();
            ui_ready = true;
            lvgl_started = true;

            // *** Long-press OTA toggle removed ***

            first = false;
            ESP_LOGI("LVGL", "UI ready");
        }

        lv_timer_handler();
        lvgl_unlock();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ------------------------------------------------------------
// Gauge Update Task (Core 1)
// ------------------------------------------------------------
void gauge_task(void *arg)
{
    GaugePacket pkt;
    // ... (previous initializations) ...

    static float coolant_lerp = 0.0f;
    static float oil_lerp = 0.0f;
    static float fuel_lerp = 0.0f;

    while (1)
    {
        // If the WiFi state is ESP_NOW_ONLY, we are in Telemetry mode.
        // If it is CONNECTED, we are likely in OTA mode.
        if (get_wifi_state() == APP_WIFI_STATE_ESP_NOW_ONLY)
        {
            gauge_state_get(&pkt);

            // Calculate LERP (Smoothing)
            coolant_lerp = lerp(coolant_lerp, (float)pkt.coolantTemp, 0.15f);
            oil_lerp = lerp(oil_lerp, (float)pkt.oilPressure, 0.15f);
            fuel_lerp = lerp(fuel_lerp, (float)pkt.fuelLevel, 0.15f);

            lvgl_lock();
            // Update functions now handle conditional logic internally
            update_coolant_meter((int32_t)coolant_lerp);
            update_oil_pressure_meter((int32_t)oil_lerp);
            update_fuel_arc((int32_t)fuel_lerp);
            lvgl_unlock();
            vTaskDelay(pdMS_TO_TICKS(16));
        }
        else
        {
            // We are in OTA mode (or transitioning), pause telemetry to save resources.
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void monitor_task(void *arg)
{
    const esp_partition_t *running = esp_ota_get_running_partition();

    while (1)
    {
        char ip_addr_str[16] = "N/A";
        get_wifi_ip_str(ip_addr_str, sizeof(ip_addr_str));

        ESP_LOGI(TAG, "WiFi State: %s | IP: %s | Partition: %s",
                 wifi_state_to_str(get_wifi_state()), ip_addr_str, running->label);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// void Driver_Loop(void *parameter)
// {
//     while (1)
//     {
//         // QMI8658_Loop();
//         // PCF85063_Loop();
//         // BAT_Get_Volts();
//         // PWR_Loop();
//         vTaskDelay(pdMS_TO_TICKS(100));
//     }
// }

void Driver_Init(void)
{
    // PWR_Init();
    // BAT_Init();
    I2C_Init();
    EXIO_Init();
    // PCF85063_Init();
    // QMI8658_Init();

    // xTaskCreatePinnedToCore(
    //     Driver_Loop,
    //     "Driver Loop",
    //     4096,
    //     NULL,
    //     3,
    //     NULL,
    //     0);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "Starting Truck Gauge Cluster");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
        running->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX)
    {
        ESP_LOGI(TAG, "Running from OTA slot, validating image...");
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }

    network_setup();

    init_wifi_state_machine();

    Driver_Init();

    LCD_Init();
    Set_Backlight(100);

    LVGL_Init();

    // Bind the hardware instantiation handle to the active LVGL driver mapping
    if (disp != NULL && disp->driver != NULL)
    {
        disp->driver->user_data = panel_handle;
    }

    lvgl_lock_init();

    xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl_task",
        8192,
        NULL,
        5,
        NULL,
        1);

    gauge_state_init();
    espnow_receiver_init(1);

    xTaskCreatePinnedToCore(
        gauge_task,
        "gauge_task",
        12288,
        NULL,
        5,
        NULL,
        0);

    xTaskCreate(monitor_task, "monitor_task", 4096, NULL, 1, NULL);
}
