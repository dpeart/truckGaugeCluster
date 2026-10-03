#include "updateUI.h"
#include "small_gauge.h"
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
        lv_meter_set_indicator_value(objects.trans_temp, screen_main_state.trans_temp, new_val / INT_SCALING);
        cached_trans = new_val;
    }
}

void update_fuel_pressure_meter(int32_t new_val) {
    // Scale down from backend resolution (scaled by 100) to match meter range
    int32_t display_val = (new_val + 5) / 100;

    if (abs(display_val - cached_fuel_pressure) > UPDATE_THRESHOLD) {
        // Ensure you pass your actual meter indicator handle as the second argument
        lv_meter_set_indicator_value(objects.fuel_pressure, screen_main_state.fuel_pressure, display_val);
        cached_fuel_pressure = display_val;
    }
}

// Single entry point called by gauge_task
void gauge_ui_update(bool is_stale) {
    (void)is_stale; // keep parameter for global stale handling if desired

    // Call per-gauge draw functions while LVGL is locked by the caller.
    // Each gauge module performs its own smoothing and calls the update_* helpers above.
    small_gauge_draw();

    // Optional: global stale indicator (if you want a single indicator)
    // if (is_stale) { show_global_stale_indicator(true); } else { show_global_stale_indicator(false); }
}
