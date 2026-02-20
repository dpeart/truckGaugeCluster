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
// #include "SD_MMC.h"   // (optional, init commented out)
#include "Wireless.h" // radio code
#include "TCA9554PWR.h"
#include "BAT_Driver.h"
#include "PWR_Key.h"

// nvs_flash.h was already included above; no need to pull it twice

#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"
#include "lvgl.h"
// #include "esp_lvgl_port.h"
#include "LVGL_Driver.h" // <-- THIS IS THE FIX YOU NEEDED

// UI + app headers
#include "src/ui/ui.h"
#include "src/updateUI.h"
#include "src/GaugePacket.h"
#include "src/espnow_receiver.h"
#include "src/lvgl_lock.h"

// Allocate full double buffer size in PSRAM
#define LVGL_BUF_LEN (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT)

static const char *TAG = "MAIN";

static void lvgl_task(void *arg)
{
    static bool first = true;

    while (1)
    {
        lvgl_lock();
        lv_timer_handler();

        if (first)
        {
            ESP_LOGI("LVGL", "creating UI");
            ui_init();
            ui_tick();
            ui_ready = true;     // now the UI really exists
            lvgl_started = true; // LVGL + UI are both ready
            first = false;
            ESP_LOGI("LVGL", "set flags: ui_ready=%d lvgl_started=%d &ui_ready=%p &lvgl_started=%p",
                     ui_ready, lvgl_started, &ui_ready, &lvgl_started);
        }

        lvgl_unlock();

        vTaskDelay(pdMS_TO_TICKS(5));
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

    // Because guage_task starts before the UI is fully initialized, we wait here until the UI signals it's ready for updates.
    // This prevents us from trying to update LVGL objects that haven't been created yet.
    while (!ui_ready || !lvgl_started)
    {
        ESP_LOGI("GAUGE", "wait: ui_ready=%d lvgl_started=%d &ui_ready=%p &lvgl_started=%p",
                 ui_ready, lvgl_started, &ui_ready, &lvgl_started);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    while (1)
    {

        gauge_state_get(&pkt);

        lvgl_lock();

        if (pkt.coolantTemp != last_coolant)
        {
            ESP_LOGI(TAG, "lastCoolant: %d, currentCoolant: %d", last_coolant, pkt.coolantTemp);

            coolant_display = coolant_display * 0.85f + pkt.coolantTemp * 0.15f;

            meter_anim_cb(objects.coolant_temp,
                          screen_main_state.coolant_temp,
                          (int)coolant_display);

            last_coolant = pkt.coolantTemp;
        }

        if (pkt.oilPressure != last_oil_pressure)
        {
            ESP_LOGI(TAG, "lastOilPressure: %d, currentOilPressure: %d", last_oil_pressure, pkt.oilPressure);

            oil_pressure_display = oil_pressure_display * 0.85f + pkt.oilPressure * 0.15f;

            meter_anim_cb(objects.oil_pressure,
                          screen_main_state.oil_pressure,
                          (int)oil_pressure_display);

            last_oil_pressure = pkt.oilPressure;
        }

        if (pkt.fuelLevel != last_fuel_level)
        {
            ESP_LOGI(TAG, "lastfuel_level: %d, currentfuel_level: %d", last_fuel_level, pkt.fuelLevel);

            fuel_level_display = fuel_level_display * 0.85f + pkt.fuelLevel * 0.15f;

            arc_anim_cb(objects.fuel_level, (int)fuel_level_display);

            last_fuel_level = pkt.fuelLevel;
        }
        lvgl_unlock();

        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

// ------------------------------------------------------------
// Driver Loop (unchanged)
// ------------------------------------------------------------
void Driver_Loop(void *parameter)
{
    // Wireless_Init();

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
    // Flash_Searching();  // SD card support is disabled for now; un-comment if you need filesystem access
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
    esp_log_level_set("*", ESP_LOG_NONE);    // silence everything
    esp_log_level_set("LVGL", ESP_LOG_INFO); // enable only LVGL logs

    ESP_LOGI(TAG, "Starting Truck Gauge Cluster");

    // Initialize NVS as wifi needs it
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Give panel power rails, SPI, and touch time to stabilize
    // vTaskDelay(pdMS_TO_TICKS(50));

    ESP_ERROR_CHECK(ret);
    Driver_Init();
    // SD card support is disabled for now; un-comment if you need filesystem access
    // SD_Init();
    LCD_Init();

    // Backlight_Init();
    Set_Backlight(100);

    LVGL_Init();
    lvgl_lock_init(); // <-- add this

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

    espnow_receiver_init(1);

    gauge_state_init();

    // Gauge update task on Core 0
    xTaskCreatePinnedToCore(
        gauge_task,
        "gauge_task",
        12288,
        NULL,
        5,
        NULL,
        0);
}