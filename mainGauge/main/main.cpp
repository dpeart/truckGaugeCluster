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
    static float speed_display = 0.0f;
    static float tach_display = 0.0f;

    for (;;)
    {
        // Non-LVGL work first
        espnow_receiver_pump(); // uses lv_tick_get only – OK
        gauge_state_get(pkt);   // mutex-protected – OK

        lvgl_port_lock(-1);

        // LVGL housekeeping
        lv_timer_handler();

        // UI updates
        if (pkt.speed != last_speed)
        {
            speed_display = speed_display * 0.85f + pkt.speed * 0.15f;
            speed_anim_cb(screen_main_state.speed_indicator, (int)speed_display);
            last_speed = pkt.speed;
        }

        if (pkt.rpm != last_rpm)
        {
            tach_display = tach_display * 0.85f + pkt.rpm * 0.15f;
            tach_anim_cb(screen_main_state.tach_indicator, (int)tach_display);
            last_rpm = pkt.rpm;
        }

        updateIndicators(pkt); // if this touches LVGL, it’s now safe
        incrementOdometer();   // same here

        lvgl_port_unlock();

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

    // Disable any pointer-type indev (GT911) created by the BSP
    // lv_indev_t *indev = NULL;
    // for (indev = lv_indev_get_next(NULL); indev != NULL; indev = lv_indev_get_next(indev))
    // {
    //     if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER)
    //     {
    //         lv_indev_delete(indev);
    //         break;
    //     }
    // }

    bsp_display_backlight_on();

    bsp_display_lock(0);
    ui_init();
    ui_tick();
    bsp_display_unlock();

    // LVGL task on Core 1
    xTaskCreatePinnedToCore(
        lvgl_tick_task,
        "lvgl_tick",
        2048,
        NULL,
        1, // low priority is fine
        NULL,
        1 // Core 1 (same as LVGL)
    );

    xTaskCreatePinnedToCore(
        runLVGLTask,
        "runLVGLTask",
        12288,
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
