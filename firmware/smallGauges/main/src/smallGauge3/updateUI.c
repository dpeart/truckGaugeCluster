#include "updateUI.h"
#include <math.h>

// Helper for Linear Interpolation
static inline float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

// Static cache to prevent redundant LVGL meter redraws
static int32_t cached_trans = -999;
static int32_t cached_fuel_pressure = -999;

void update_trans_temp_meter(int32_t new_val) {
    if (abs(new_val - cached_trans) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.trans_temp, screen_main_state.trans_temp, new_val);
        cached_trans = new_val;
    }
}

void update_fuel_pressure_meter(int32_t new_val) {
    if (abs(new_val - cached_fuel_pressure) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.fuel_pressure, screen_main_state.fuel_pressure, new_val);
        cached_fuel_pressure = new_val;
    }
}

// Single entry point called by gauge_task
void gauge_ui_update(const GaugePacket *pkt, bool is_stale) {
    static float trans_lerp = 0.0f;
    static float fuel_pressure_lerp = 0.0f;

    // 1. Calculate LERP smoothing
    trans_lerp = lerp(trans_lerp, (float)pkt->transTemp, 0.15f);
    fuel_pressure_lerp = lerp(fuel_pressure_lerp, (float)pkt->fuelPressure, 0.15f);

    // 2. Dispatch to LVGL update functions
    update_trans_temp_meter((int32_t)trans_lerp);
    update_fuel_pressure_meter((int32_t)fuel_pressure_lerp);
}