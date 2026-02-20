// #define DEBUG 1       // unused; esp-idf logging levels are configured via menuconfig
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_now.h"

#include "ST77916.h"  // LCD driver
#include "PCF85063.h" // RTC
#include "QMI8658.h"  // IMU
#include "SD_MMC.h"   // (optional, init commented out)
#include "Wireless.h" // radio code
#include "TCA9554PWR.h"
#include "BAT_Driver.h"
#include "PWR_Key.h"

// nvs_flash.h was already included above; no need to pull it twice

#define LV_CONF_INCLUDE_SIMPLE
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "LVGL_Driver.h" // <-- THIS IS THE FIX YOU NEEDED

// UI + app headers
#include "src/ui/ui.h"
#include "src/updateUI.h"
#include "src/GaugePacket.h"
#include "src/espnow_receiver.h"
// espnow_task isn't currently used; receiver updates global state directly
#include "src/lvgl_lock.h"

static const char *TAG = "MAIN";

// Provided by LCD driver (you do NOT own these)
extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_panel_handle_t panel_handle;
extern esp_lcd_touch_handle_t tp;

static void lvgl_task(void *arg)
{
    while (1)
    {
        lvgl_lock();
        lv_timer_handler();
        lvgl_unlock();

        vTaskDelay(pdMS_TO_TICKS(5)); // 5–10 ms is typical
    }
}

// ------------------------------------------------------------
// Gauge Update Task (Core 1)
// ------------------------------------------------------------
void gauge_task(void *arg)
{
    GaugePacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    int32_t last_coolant = -1;
    int32_t last_oil_pressure = -1;
    int32_t last_fuel_level = -1;

    static float coolant_display = 0.0f;
    static float oil_pressure_display = 0.0f;
    static float fuel_level_display = 0.0f;

    while (1)
    {
        // NEW: no pump call needed
        gauge_state_get(&pkt);

        lvgl_lock();

        if (pkt.coolantTemp != last_coolant)
        {
            ESP_LOGI(TAG, "lastCoolant: %d, currentCoolant: %d", last_coolant, pkt.coolantTemp);

            coolant_display = coolant_display * 0.85f + pkt.coolantTemp * 0.15f;
            coolant_anim_cb(screen_main_state.coolant_temp, (int)coolant_display);
            last_coolant = pkt.coolantTemp;
        }

        if (pkt.oilPressure != last_oil_pressure)
        {
            ESP_LOGI(TAG, "lastOilPressure: %d, currentOilPressure: %d", last_oil_pressure, pkt.oilPressure);
            oil_pressure_display = oil_pressure_display * 0.85f + pkt.oilPressure * 0.15f;
            oil_pressure_anim_cb(screen_main_state.oil_pressure, (int)oil_pressure_display);
            last_oil_pressure = pkt.oilPressure;
        }

        if (pkt.fuelLevel != last_fuel_level)
        {
            ESP_LOGI(TAG, "lastfuel_level: %d, currentfuel_level: %d", last_fuel_level, pkt.fuelLevel);
            fuel_level_display = fuel_level_display * 0.85f + pkt.fuelLevel * 0.15f;
            fuel_level_anim_cb(objects.fuel_level, (int)fuel_level_display);
            last_fuel_level = pkt.fuelLevel;
        }

        lvgl_unlock();

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ------------------------------------------------------------
// Driver Loop (unchanged)
// ------------------------------------------------------------
void Driver_Loop(void *parameter)
{
    Wireless_Init();

    while (1)
    {
        QMI8658_Loop();
        PCF85063_Loop();
        BAT_Get_Volts();
        PWR_Loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Driver_Init(void)
{
    PWR_Init();
    BAT_Init();
    I2C_Init();
    EXIO_Init();
    Flash_Searching();
    PCF85063_Init();
    QMI8658_Init();

    xTaskCreatePinnedToCore(
        Driver_Loop,
        "Driver Loop",
        4096,
        NULL,
        3,
        NULL,
        0);
}

// ------------------------------------------------------------
// app_main
// ------------------------------------------------------------
void app_main(void)
{
    // esp_log_level_set("*", ESP_LOG_NONE);
    ESP_LOGI(TAG, "Starting Truck Gauge Cluster");

    // Initialize NVS as wifi needs it
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Give panel power rails, SPI, and touch time to stabilize
    vTaskDelay(pdMS_TO_TICKS(250));

    ESP_ERROR_CHECK(ret);
    espnow_receiver_init(1);

    gauge_state_init();

    Driver_Init();
    // SD card support is disabled for now; un-comment if you need filesystem access
    // SD_Init();
    LCD_Init();

    LVGL_Init();

    // Start LVGL task
    xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl_task",
        4096,
        NULL,
        5,
        NULL,
        1 // or 0, just be consistent with your other tasks
    );

    lvgl_lock();
    ui_init();
    ui_tick();
    lvgl_unlock();

    // Gauge update task on Core 0
    xTaskCreatePinnedToCore(
        gauge_task,
        "gauge_task",
        12288,
        NULL,
        3,
        NULL,
        0);

    // while (1)
    // {
    //     vTaskDelay(pdMS_TO_TICKS(10));
    // }
}