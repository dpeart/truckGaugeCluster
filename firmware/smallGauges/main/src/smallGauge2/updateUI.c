#include "updateUI.h"
#include "small_gauge.h"
#include <math.h>

// Helper for Linear Interpolation
static inline float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

static int32_t cached_iat = -999;
static int32_t cached_egt = -999;
static int32_t cached_battery = -999;

void update_iat_meter(int32_t new_val)
{
    if (abs(new_val - cached_iat) > UPDATE_THRESHOLD)
    {
        int32_t display_val = new_val / INT_SCALING; // e.g., 7243 -> 72°F
        lv_meter_set_indicator_value(objects.iat, screen_main_state.iat_temp, display_val);
        cached_iat = new_val;
    }
}

void update_egt_meter(int32_t new_val)
{
    if (abs(new_val - cached_egt) > UPDATE_THRESHOLD)
    {
        int32_t display_val = new_val / INT_SCALING; // e.g., 7243 -> 72°F
        lv_meter_set_indicator_value(objects.egt, screen_main_state.egt_temp, display_val);
        cached_egt = new_val;
    }
}

void update_battery_arc(int32_t new_val)
{
    // Convert raw backend value (0 - 1600) to match LVGL arc range (0 - 16)
    int32_t display_val = (new_val + 5) / 100;

    if (abs(display_val - cached_battery) > UPDATE_THRESHOLD)
    {
        lv_arc_set_value(objects.battery, display_val);
        cached_battery = display_val;
    }
}

// Single entry point called by gauge_task
void gauge_ui_update(bool is_stale)
{
    (void)is_stale; // keep parameter for global stale handling if desired

    // Call per-gauge draw functions while LVGL is locked by the caller.
    // Each gauge module performs its own smoothing and calls the update_* helpers above.
    small_gauge_draw();

    // Optional: global stale indicator (if you want a single indicator)
    // if (is_stale) { show_global_stale_indicator(true); } else { show_global_stale_indicator(false); }
}
