#include "updateUI.h"
#include <math.h>

// Helper for Linear Interpolation
static inline float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

static int32_t cached_iat = -999;
static int32_t cached_egt = -999;
static int32_t cached_battery = -999;

void update_iat_meter(int32_t new_val) {
    if (abs(new_val - cached_iat) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.iat, screen_main_state.iat_temp, new_val);
        cached_iat = new_val;
    }
}

void update_egt_meter(int32_t new_val) {
    if (abs(new_val - cached_egt) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.egt, screen_main_state.egt_temp, new_val);
        cached_egt = new_val;
    }
}

void update_battery_arc(int32_t new_val) {
    if (abs(new_val - cached_battery) > UPDATE_THRESHOLD) {
        lv_arc_set_value(objects.battery, new_val);
        cached_battery = new_val;
    }
}
// Single entry point called by gauge_task
void gauge_ui_update(const GaugePacket *pkt, bool is_stale) {
    static float iat_lerp = 0.0f;
    static float egt_lerp = 0.0f;
    static float battery_lerp = 0.0f;

    // 1. Calculate LERP smoothing
    iat_lerp     = lerp(iat_lerp, (float)pkt->iaTemp, 0.15f);
    egt_lerp     = lerp(egt_lerp, (float)pkt->EGTemp, 0.15f);
    battery_lerp = lerp(battery_lerp, (float)pkt->batteryLevel, 0.15f);

    // 2. Dispatch to LVGL update functions
    update_iat_meter((int32_t)iat_lerp);
    update_egt_meter((int32_t)egt_lerp);
    update_battery_arc((int32_t)battery_lerp);
}