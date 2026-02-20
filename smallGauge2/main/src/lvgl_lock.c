#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

static SemaphoreHandle_t s_lvgl_mutex = NULL;

void lvgl_lock_init(void)
{
    if (s_lvgl_mutex == NULL) {
        s_lvgl_mutex = xSemaphoreCreateMutex();
    }
}

void lvgl_lock(void)
{
    if (s_lvgl_mutex) {
        xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
    }
}

void lvgl_unlock(void)
{
    if (s_lvgl_mutex) {
        xSemaphoreGive(s_lvgl_mutex);
    }
}