// updateUI.c
#include "updateUI.h"
#include "small_gauge.h" // per-gauge module(s)
#include <math.h>
#include "lvgl.h" // ensure LVGL types are available for update helpers
#include "ui.h"   // your objects/screen state headers (adjust to your project)

// Helper for Linear Interpolation (kept for any local use)
static inline float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

// Static state tracking used by the LVGL update helpers
static int32_t cached_coolant = -999;
static int32_t cached_oil = -999;
static int32_t cached_fuel = -999;

void update_coolant_meter(int32_t new_val)
{
    // If UPDATE_THRESHOLD is in scaled units (e.g., 100 = 1°F change required to redraw)
    if (abs(new_val - cached_coolant) > UPDATE_THRESHOLD)
    {
        int32_t display_val = new_val / INT_SCALING; // e.g., 7243 / 100 = 72°F
        lv_meter_set_indicator_value(objects.coolant_temp, screen_main_state.coolant_temp, display_val);
        cached_coolant = new_val;
    }
}

// updateUI.c
void update_oil_pressure_meter(int32_t new_val)
{
    // Scale down from high-precision backend (e.g. 4500 -> 45 PSI) with rounding
    int32_t display_val = (new_val + 50) / 100;

    if (abs(display_val - cached_oil) > UPDATE_THRESHOLD)
    {
        lv_meter_set_indicator_value(objects.oil_pressure, screen_main_state.oil_pressure, display_val);
        cached_oil = display_val;
    }
}

void update_fuel_arc(int32_t new_val)
{
    // Scale down from high-precision backend range (0-10000) to LVGL arc range (0-100) with rounding
    int32_t display_val = (new_val + 50) / 100;

    if (abs(display_val - cached_fuel) > UPDATE_THRESHOLD)
    {
        lv_arc_set_value(objects.fuel_level, display_val);
        cached_fuel = display_val;
    }
}

// Single entry point called by gauge_task
// Per-gauge modules perform smoothing and read their own subscribed state.
void gauge_ui_update(bool is_stale)
{
    (void)is_stale; // keep parameter for global stale handling if desired

    // Call per-gauge draw functions while LVGL is locked by the caller.
    // Each gauge module performs its own smoothing and calls the update_* helpers above.
    small_gauge_draw();
}