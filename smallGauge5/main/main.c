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

    int32_t last_ambientTemp = -1;
    static char last_compass_str[4] = "";
    static char last_time_str[10] = "";

    while (!ui_ready || !lvgl_started)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    while (1)
    {
        gauge_state_get(&pkt);
        lvgl_lock();

        // --- Ambient Temp (Now as String) ---
        if (pkt.ambientTemp != last_ambientTemp)
        {
            char temp_buf[12];
            snprintf(temp_buf, sizeof(temp_buf), "%d", pkt.ambientTemp);
            text_update_cb(objects.ambient_temp, temp_buf);

            last_ambientTemp = pkt.ambientTemp;
        }

        // --- Compass Logic (String Comparison) ---
        // Assuming pkt.compass8 is a char array or pointer in your GaugePacket
        if (strcmp(pkt.compass8, last_compass_str) != 0)
        {
            // 1. Log the change
            ESP_LOGI(TAG, "Compass changed from %s to %s", last_compass_str, pkt.compass8);

            // 2. Update the UI using your unified string callback
            // We add the degree symbol here if you want it, or just pass raw string
            char comp_buf[8];
            snprintf(comp_buf, sizeof(comp_buf), "%s", pkt.compass8);
            text_update_cb(objects.heading, comp_buf);

            // 3. Sync the tracking variable
            strncpy(last_compass_str, pkt.compass8, sizeof(last_compass_str) - 1);
        }

        // --- Clock ---
        char current_time_str[10];

        // Convert 0-23 hour to 1-12 hour
        int display_hour = pkt.hour % 12;
        if (display_hour == 0)
            display_hour = 12;

        // %d for hour (no leading zero), %02d for minute (always two digits)
        snprintf(current_time_str, sizeof(current_time_str), "%d:%02d", display_hour, pkt.minute);

        if (strcmp(current_time_str, last_time_str) != 0)
        {
            text_update_cb(objects.time, current_time_str);
            strcpy(last_time_str, current_time_str);
        }
        // --- Update Indicators ---

        updateIndicators(&pkt);

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
    esp_log_level_set("*", ESP_LOG_NONE); // silence everything
    // esp_log_level_set("LVGL", ESP_LOG_INFO); // enable only LVGL logs

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