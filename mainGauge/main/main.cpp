#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "images.h"

#include <lvgl.h>
#include "lv_demos.h"

#include "src/ui/ui.h"
#include "src/updateSpeed.h"
#include "src/GaugePacket.h"
// #include "src/espnow_receiver.h"
// #include "src/espnow_task.h"

#include "SlaveBootHandler.h"
#include "p4_uart.h"
#include "p4_modes.h"
#include "p4_ota.h"

static const char *TAG = "P4_MAIN";

static SlaveBootHandler g_boot;

// ------------------------------------------------------------
// LVGL Task (Core 1)
// ------------------------------------------------------------

void lvgl_tick_task(void *arg)
{
    while (1)
    {
        lv_tick_inc(1); // advance LVGL time by 1 ms
        vTaskDelay(1);  // sleep 1 ms
    }
}
void runLVGLTask(void *arg)
{
    GaugePacket pkt{};
    int32_t last_speed = -1;
    int32_t last_rpm = -1;
    // static float speed_display = 0.0f;
    // static float tach_display = 0.0f;

    for (;;)
    {
        // 1. Non-LVGL work first
        gauge_state_get(pkt); // mutex-protected – OK

        // 2. Lock the port to interact with LVGL safely
        lvgl_port_lock(-1);

        // --- START PERFORMANCE BENCHMARK ---
        TickType_t start_tick = xTaskGetTickCount();

        // 3. UI updates (Setting values only flags them as dirty)
        if (pkt.speed != last_speed)
        {
            update_speed_ui(last_speed, pkt.speed);
            last_speed = pkt.speed;
        }

        if (pkt.rpm != last_rpm)
        {
            update_tach_ui(last_rpm, pkt.rpm);
            last_rpm = pkt.rpm;
        }

        updateIndicators(pkt);
        incrementOdometer();

        // 4. Run housekeeping and get the recommended sleep time
        uint32_t time_till_next = lv_timer_handler();

        // --- END PERFORMANCE BENCHMARK ---
        TickType_t end_tick = xTaskGetTickCount();

        // 5. Release the graphics lock
        lvgl_port_unlock();

        // Calculate how long the CPU spent changing data and rendering pixels
        uint32_t execution_time = (end_tick - start_tick) * portTICK_PERIOD_MS;

        // LOGGING: Warn if our combined data change + render blew past the 16ms budget
        if (execution_time > 20)
        {
            ESP_LOGW(TAG, "Frame dropped! Processing took %lu ms (Target: <20ms)", execution_time);
        }
        // 6. DYNAMIC THROTTLE:
        // Cap the maximum frame rate so it doesn't try to loop infinitely.
        if (time_till_next < 20)
        {
            time_till_next = 20;
        }

        // Smart Sleep Adjustment:
        // Account for the time we *already spent* processing this frame.
        if (time_till_next > execution_time)
        {
            time_till_next -= execution_time;
        }
        else
        {
            time_till_next = 1; // We are behind schedule! Yield briefly, then loop immediately.
        }

        // Delay based on what LVGL actually needs, keeping the CPU completely asleep
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

// ------------------------------------------------------------
// app_main (C++ entry point)
// ------------------------------------------------------------
extern "C" void app_main(void)
{
    // Disable messaging
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("P4_UART", ESP_LOG_INFO);
    esp_log_level_set("UI", ESP_LOG_INFO);
    esp_log_level_set("P4_TELEM", ESP_LOG_NONE);
    esp_log_level_set("STATE", ESP_LOG_INFO);
    esp_log_level_set("P4_MAIN", ESP_LOG_INFO);

    esp_log_level_set("*", ESP_LOG_NONE);

    ESP_LOGI(TAG, "Starting Truck Gauge Cluster");

    // ---------------------------------------------------------
    // 1. Boot handler FIRST (controls EN/IO9 + monitors GPIO51)
    // ---------------------------------------------------------
    g_boot.begin(5);

    // ---------------------------------------------------------
    // 2. Initialize global gauge state BEFORE UART RX starts
    // ---------------------------------------------------------
    gauge_state_init();

    // ---------------------------------------------------------
    // 3. Initialize OTA system (queue + task)
    // ---------------------------------------------------------
    p4_ota_init();

    // ---------------------------------------------------------
    // 4. Start UART RX task (P4 UART pipeline)
    // ---------------------------------------------------------
    start_uart_rx_task();

    // Give UART RX task time to start
    vTaskDelay(pdMS_TO_TICKS(20));

    // ---------------------------------------------------------
    // 5. Initial PING to C6
    // ---------------------------------------------------------
    uart_send_frame(CMD_PING, nullptr, 0);

    // Start in Telemetry mode
    current_mode = p4_mode_t::TELEMETRY;

    // ---------------------------------------------------------
    // 6. Display + LVGL initialization (High-Performance Internal SRAM Strategy)
    // ---------------------------------------------------------

// 1. Undefine the full-screen macros so we can substitute our optimized version
#ifdef BSP_LCD_DRAW_BUFF_SIZE
#undef BSP_LCD_DRAW_BUFF_SIZE
#endif

#ifdef BSP_LCD_DRAW_BUFF_DOUBLE
#undef BSP_LCD_DRAW_BUFF_DOUBLE
#endif

// 2. Define a height that is a fraction of the screen (e.g., 100 rows out of 800)
// This forces the allocation to easily fit inside ultra-fast internal SRAM.
#define LVGL_DRAW_BUF_HEIGHT 200
#define BSP_LCD_DRAW_BUFF_SIZE (BSP_LCD_H_RES * LVGL_DRAW_BUF_HEIGHT)
#define BSP_LCD_DRAW_BUFF_DOUBLE 0 // 0 = Single partial buffer, perfect for internal SRAM

    ESP_LOGI("MAIN", "Initializing display with fast internal SRAM chunks (%d pixels)...", BSP_LCD_DRAW_BUFF_SIZE);

    // 3. Call the public BSP startup function.
    // It looks at our modified macro sizes, allocates the internal memory,
    // and registers the LVGL v8.4 driver layout completely automatically.
    auto *disp = bsp_display_start();

    if (disp == NULL)
    {
        ESP_LOGE("MAIN", "BSP Display initialization failed!");
        return;
    }

    ESP_LOGI("MAIN", "Display and graphics engine successfully running via internal SRAM!");

    // ---------------------------------------------------------
    // 7. Dynamic Asset Optimization: Move Background to PSRAM
    // ---------------------------------------------------------

    // Calculate the byte size of the 800x800 background image (RGB565 = 2 bytes per pixel)
    size_t bg_image_bytes = 800 * 800 * 2;

    ESP_LOGI("MAIN", "Allocating %.2f MB in PSRAM for background...", (float)bg_image_bytes / (1024.0f * 1024.0f));

    // Allocate space in external PSRAM (SPIRAM)
    uint8_t *psram_bg_buffer = (uint8_t *)heap_caps_malloc(bg_image_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (psram_bg_buffer == NULL)
    {
        ESP_LOGE("MAIN", "Failed to allocate PSRAM for background image! Leaving asset in slow Flash.");
    }
    else
    {
        // Copy the image data straight from its Flash address to fast PSRAM
        ESP_LOGI("MAIN", "Copying background data from Flash to PSRAM...");
        memcpy(psram_bg_buffer, img_main_gauge_bg.data, bg_image_bytes);

        // CREATE A WRITABLE COPY:
        // Create a non-const copy of the structure in standard runtime RAM
        static lv_img_dsc_t optimized_bg_desc;
        optimized_bg_desc = img_main_gauge_bg;    // Copy headers, width, height, dimensions
        optimized_bg_desc.data = psram_bg_buffer; // Overwrite the source pointer to our PSRAM buffer

        // REDIRECT THE WIDGET POINTER:
        // Find where you set your background widget image asset in main.cpp
        // (usually look for something like: lv_img_set_src(ui_img_bg_widget, &img_main_gauge_bg); )
        // You MUST change that setup call to use our new writable copy instead:
        //
        // lv_img_set_src(your_background_obj, &optimized_bg_desc);

        ESP_LOGI("MAIN", "Background asset successfully duplicated to high-speed PSRAM!");
    }

    // Turn on the backlight now that initialization succeeded
    bsp_display_backlight_on();

    bsp_display_lock(0);
    ui_init();
    ui_tick();
    bsp_display_unlock();

    // ---------------------------------------------------------
    // 7. LVGL tick + render tasks (Core 1)
    // ---------------------------------------------------------
    xTaskCreatePinnedToCore(
        lvgl_tick_task,
        "lvgl_tick",
        2048,
        NULL,
        2, // slightly higher priority
        NULL,
        1 // Core 1
    );

    xTaskCreatePinnedToCore(
        runLVGLTask,
        "runLVGLTask",
        12288,
        nullptr,
        10,
        nullptr,
        1 // Core 1
    );

    // ---------------------------------------------------------
    // 8. Idle loop
    // ---------------------------------------------------------
    while (true)
    {
        vTaskDelay(10);
    }
}