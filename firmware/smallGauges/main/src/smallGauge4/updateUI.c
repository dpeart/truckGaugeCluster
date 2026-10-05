// updateUI.c
#include "updateUI.h"
#include "small_gauge.h"
#include "lvgl.h"
#include "ui.h"
#include <math.h>
#include <stdlib.h>

static int32_t cached_oil_temp = -999;
static int32_t cached_boost = -999;

void update_oil_temp_meter(int32_t new_val) {
    if (abs(new_val - cached_oil_temp) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.oil_temp, screen_main_state.oil_temp, new_val / INT_SCALING);
        cached_oil_temp = new_val;
    }
}

void update_boost_pressure_meter(int32_t new_val) {
    // Scale down from backend resolution (scaled by 100) to match meter range
    int32_t display_val = (new_val + 5) / 100;

    if (abs(display_val - cached_boost) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.boost_pressure, screen_main_state.boost_pressure, display_val);
        cached_boost = display_val;
    }
}

void gauge_ui_update(bool is_stale) {
    (void)is_stale;
    
    // Call per-gauge draw functions while LVGL lock is held by the caller
    small_gauge_draw();
}