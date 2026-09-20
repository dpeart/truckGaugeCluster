#include "updateUI.h"
#include <math.h>

// Helper for Linear Interpolation
static inline float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

// Static state tracking
static int32_t cached_coolant = -999;
static int32_t cached_oil = -999;
static int32_t cached_fuel = -999;

void update_coolant_meter(int32_t new_val) {
    if (abs(new_val - cached_coolant) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.coolant_temp, screen_main_state.coolant_temp, new_val);
        cached_coolant = new_val;
    }
}

void update_oil_pressure_meter(int32_t new_val) {
    if (abs(new_val - cached_oil) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.oil_pressure, screen_main_state.oil_pressure, new_val);
        cached_oil = new_val;
    }
}

void update_fuel_arc(int32_t new_val) {
    if (abs(new_val - cached_fuel) > UPDATE_THRESHOLD) {
        lv_arc_set_value(objects.fuel_level, new_val);
        cached_fuel = new_val;
    }
}

// Single entry point called by gauge_task
void gauge_ui_update(const GaugePacket *pkt, bool is_stale) {
    static float coolant_lerp = 0.0f;
    static float oil_lerp = 0.0f;
    static float fuel_lerp = 0.0f;

    // 1. Calculate LERP smoothing
    coolant_lerp = lerp(coolant_lerp, (float)pkt->coolantTemp, 0.15f);
    oil_lerp     = lerp(oil_lerp, (float)pkt->oilPressure, 0.15f);
    fuel_lerp    = lerp(fuel_lerp, (float)pkt->fuelLevel, 0.15f);

    // 2. Dispatch to LVGL update functions
    update_coolant_meter((int32_t)coolant_lerp);
    update_oil_pressure_meter((int32_t)oil_lerp);
    update_fuel_arc((int32_t)fuel_lerp);
}