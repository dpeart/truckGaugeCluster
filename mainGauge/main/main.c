#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include <lvgl.h>
#include "lv_demos.h"
#include "src/ui/ui.h"
#include "src/updateSpeed.h"

void runLVGLTask(void *arg)
{
    // GaugePacket gauge_render = {0};
    static int32_t last_displayed_speed = -1;

    for (;;)
    {
        // 1. Handle LVGL Timers (Animations, screen refresh, etc.)
        // Returns the number of ms until it needs to run again.
        lv_timer_handler();
        // uint32_t next_lvgl_call = lv_timer_handler();

        // 2. Wait for data in the queue OR the next LVGL refresh
        // This is the "magic" change. If next_lvgl_call is 5ms,
        // the task sleeps for 5ms UNLESS a packet arrives sooner.
        // if (xQueueReceive(g_data_queue, &gauge_render, pdMS_TO_TICKS(next_lvgl_call)) == pdTRUE)
        // {

        // 3. Only update UI if value changed
        // if (gauge_render.speed != last_displayed_speed)
        generateTurnSignalPattern();

        incrementOdometer();
        
        if (last_displayed_speed == -1)
        {

            update_speed_ui(0, 140);
            update_tach_ui(0, 50);
            // update_speed_ui(last_displayed_speed, gauge_render.speed);
            last_displayed_speed = 0;
            // last_displayed_speed = gauge_render.speed;
        }
        // }

        // Add a final safety yield if the queue is being hit hard
        vTaskDelay(1);
    }
}
void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false, // must be set to true for software rotation
        }};

    lv_display_t *disp = bsp_display_start_with_config(&cfg);

    // if (disp != NULL)
    // {
    //     bsp_display_rotate(disp, LV_DISPLAY_ROTATION_90); // 90、180、270
    // }

    bsp_display_backlight_on();

    bsp_display_lock(0);

    ui_init();

    ui_tick();

    // Create a task to run lvgl on core 1
    xTaskCreatePinnedToCore(
        runLVGLTask,
        "runLVGLTask",
        4 * 1024,
        NULL,
        10,
        NULL,
        1);

    while (1)
    {
        // static int current_speed = -1;
        // if (current_speed < 0)
        // {
        //     current_speed = 0;
        //     static int target_speed = 140;
        //     update_speed_ui(current_speed, target_speed);
        // // }
        // lv_timer_handler();
        vTaskDelay(1);
    }

    bsp_display_unlock();
}
