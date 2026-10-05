#include "updateUI.h"
#include "small_gauge.h" // per-gauge module(s)
#include <math.h>
#include <stdlib.h>
#include "lvgl.h" // ensure LVGL types are available for update helpers
#include "ui.h"   // your objects/screen state headers (adjust to your project)

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
    int32_t display_val = new_val / INT_SCALING; // Convert 5000->50°F, 20000->200°F

    // Check delta against actual integer display degrees
    if (abs(display_val - cached_iat) >= 1)
    {
        lv_meter_set_indicator_value(objects.iat, screen_main_state.iat_temp, display_val);
        cached_iat = display_val;
    }
}

void update_egt_meter(int32_t new_val)
{
    int32_t display_val = new_val / INT_SCALING; // Convert 20000->200°F, 140000->1400°F

    if (abs(display_val - cached_egt) >= 1)
    {
        lv_meter_set_indicator_value(objects.egt, screen_main_state.egt_temp, display_val);
        cached_egt = display_val;
    }
}

void update_battery_arc(int32_t new_val)
{
    // Divide by 10 to get tenths of a volt (1150 raw -> 115 [11.5V])
    int32_t display_val = new_val / 10;

    if (display_val != cached_battery)
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
}