// updateUI.c
#include "updateUI.h"
#include "small_gauge.h"
#include "lvgl.h"
#include "ui.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "updateUI";

static int32_t cached_oil_temp = -999;
static int32_t cached_boost = -999;

void update_oil_temp_meter(int32_t new_val) {
    int32_t delta = abs(new_val - cached_oil_temp);
    int32_t target_indicator_val = new_val / INT_SCALING; // Note: checks double scaling if new_val was already scaled down!

    if (delta > UPDATE_THRESHOLD) {
        ESP_LOGI(TAG, "OIL TEMP UPDATE  -> Recv: %d | Cached: %d | Delta: %d | LVGL Indicator Val: %d",
                 new_val, cached_oil_temp, delta, target_indicator_val);

        lv_meter_set_indicator_value(objects.oil_temp, screen_main_state.oil_temp, target_indicator_val);
        cached_oil_temp = new_val;
    } else {
        ESP_LOGD(TAG, "OIL TEMP IGNORED -> Recv: %d | Cached: %d | Delta: %d <= Threshold (%d)",
                 new_val, cached_oil_temp, delta, UPDATE_THRESHOLD);
    }
}

void update_boost_pressure_meter(int32_t new_val) {
    // Scale down from backend resolution (scaled by 100) to match meter range
    int32_t display_val = (new_val + 5) / 100;
    int32_t delta = abs(display_val - cached_boost);

    if (delta > UPDATE_THRESHOLD) {
        ESP_LOGI(TAG, "BOOST PRESS UPDATE -> Recv: %d | DisplayVal: %d | Cached: %d | Delta: %d | LVGL Indicator Val: %d",
                 new_val, display_val, cached_boost, delta, display_val);

        lv_meter_set_indicator_value(objects.boost_pressure, screen_main_state.boost_pressure, display_val);
        cached_boost = display_val;
    } else {
        ESP_LOGD(TAG, "BOOST PRESS IGNORED -> Recv: %d | DisplayVal: %d | Cached: %d | Delta: %d <= Threshold (%d)",
                 new_val, display_val, cached_boost, delta, UPDATE_THRESHOLD);
    }
}

void gauge_ui_update(bool is_stale) {
    (void)is_stale;
    
    // Call per-gauge draw functions while LVGL lock is held by the caller
    small_gauge_draw();
}