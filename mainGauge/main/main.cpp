#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_hosted.h"
#include "esp_now.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"

#include <lvgl.h>
#include "lv_demos.h"

#include "src/ui/ui.h"
#include "src/updateSpeed.h"
#include "src/GaugePacket.h"
#include "src/espnow_receiver.h"
#include "src/espnow_task.h"

static const char *TAG = "MAIN";

// ------------------------------------------------------------
// LVGL Task (Core 1)
// ------------------------------------------------------------
void runLVGLTask(void *arg)
{
    GaugePacket pkt{};
    int32_t last_speed = -1;
    int32_t last_rpm = -1;

    for (;;)
    {
        // LVGL housekeeping
        lv_timer_handler();

        // Pull from UART/ESP-NOW queue → update global state
        espnow_receiver_pump();

        // Read the latest state (thread-safe)
        gauge_state_get(pkt);

        // Update UI only when values change
        if (pkt.speed != last_speed)
        {
            // ESP_LOGI("LVGL", "speed: last=%d new=%d", last_speed, pkt.speed);
            update_speed_ui(last_speed < 0 ? 0 : last_speed, pkt.speed);
            last_speed = pkt.speed;
        }

        if (pkt.rpm != last_rpm)
        {
            // ESP_LOGI("LVGL", "rpm:   last=%d new=%d", last_rpm, pkt.rpm);
            update_tach_ui(last_rpm < 0 ? 0 : last_rpm, pkt.rpm);
            last_rpm = pkt.rpm;
        }

        updateIndicators(pkt);
        // generateTurnSignalPattern();
        incrementOdometer();

        vTaskDelay(1);
    }
}

// ------------------------------------------------------------
// app_main (C++ entry point)
// ------------------------------------------------------------
extern "C" void app_main(void)
{
    // Disable messaging
    esp_log_level_set("*", ESP_LOG_NONE);

    ESP_LOGI(TAG, "Starting Truck Gauge Cluster");

    // Initialize global gauge state
    gauge_state_init();

#define BSP_LCD_DRAW_BUFF_SIZE (BSP_LCD_H_RES * BSP_LCD_V_RES)
// #define BSP_LCD_DRAW_BUFF_SIZE (BSP_LCD_H_RES * 100)
#define BSP_LCD_DRAW_BUFF_DOUBLE 1

    // Display config
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = false,
        }};

    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);
    ui_init();
    ui_tick();
    bsp_display_unlock();

    // LVGL task on Core 1
    xTaskCreatePinnedToCore(
        runLVGLTask,
        "runLVGLTask",
        4096,
        nullptr,
        10,
        nullptr,
        1);

    // ESP-NOW receiver
    espnow_receiver_init(1);

    // ESP-NOW processing task on Core 0
    // start_espnow_task(0);

    // Idle loop
    while (true)
    {
        vTaskDelay(10);
    }
}
