#include "updateUI.h"
#include "small_gauge.h"
#include "lvgl.h"
#include "ui.h"
#include <math.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "updateUI";

static int32_t cached_trans = -999;
static int32_t cached_fuel_pressure = -999;

void update_trans_temp_meter(int32_t new_val)
{
    int32_t display_val = (new_val + (INT_SCALING / 2)) / INT_SCALING;

    if (abs(display_val - cached_trans) > UPDATE_THRESHOLD)
    {
        ESP_LOGI(TAG, "Trans Temp LVGL Update: raw=%" PRId32 " -> display=%" PRId32 " (prev=%" PRId32 ")",
                 new_val, display_val, cached_trans);

        lv_meter_set_indicator_value(objects.trans_temp, screen_main_state.trans_temp, display_val);
        cached_trans = display_val;
    }
}

void update_fuel_pressure_meter(int32_t new_val)
{
    int32_t display_val = (new_val + (INT_SCALING / 2)) / INT_SCALING;

    if (abs(display_val - cached_fuel_pressure) > UPDATE_THRESHOLD)
    {
        ESP_LOGI(TAG, "Fuel Press LVGL Update: raw=%" PRId32 " -> display=%" PRId32 " (prev=%" PRId32 ")",
                 new_val, display_val, cached_fuel_pressure);

        lv_meter_set_indicator_value(objects.fuel_pressure, screen_main_state.fuel_pressure, display_val);
        cached_fuel_pressure = display_val;
    }
}

void gauge_ui_update(bool is_stale)
{
    (void)is_stale;

    // Call per-gauge draw functions while LVGL is locked by the caller.
    small_gauge_draw();
}