#include "updateUI.h"
#include <math.h>

// Helper for Linear Interpolation
static inline float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

static int32_t cached_oil = -999;
static int32_t cached_boost = -999;

void update_oil_temp_meter(int32_t new_val) {
    if (abs(new_val - cached_oil) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.oil_temp, screen_main_state.oil_temp, new_val);
        cached_oil = new_val;
    }
}

void update_boost_pressure_meter(int32_t new_val) {
    if (abs(new_val - cached_boost) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.boost_pressure, screen_main_state.boost_pressure, new_val);
        cached_boost = new_val;
    }
}

// Single entry point called by gauge_task
void gauge_ui_update(const GaugePacket *pkt, bool is_stale) {
    static float oil_temp_lerp = 0.0f;
    static float boost_lerp = 0.0f;

    // 1. Calculate LERP smoothing
    oil_temp_lerp = lerp(oil_temp_lerp, (float)pkt->oilTemp, 0.15f);
    boost_lerp    = lerp(boost_lerp, (float)pkt->boostPressure, 0.15f);

    // 2. Dispatch to LVGL update functions
    update_oil_temp_meter((int32_t)oil_temp_lerp);
    update_boost_pressure_meter((int32_t)boost_lerp);
}