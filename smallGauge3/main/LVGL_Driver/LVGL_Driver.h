#pragma once
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "ST77916.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LVGL_BUF_LEN  (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT / 20)
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

extern lv_disp_draw_buf_t disp_buf;
extern lv_disp_drv_t disp_drv;
extern lv_disp_t *disp;

void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
void example_lvgl_port_update_callback(lv_disp_drv_t *drv);
void example_increase_lvgl_tick(void *arg);

void LVGL_Init(void);

#ifdef __cplusplus
}
#endif