#include "updateUI.h"
#include "small_gauge.h"
#include "lvgl.h"
#include "ui.h"
#include <math.h>
#include <stdlib.h>

static int32_t cached_coolant = -999;
static int32_t cached_oil = -999;
static int32_t cached_fuel = -999;

void update_coolant_meter(int32_t new_val)
{
    if (abs(new_val - cached_coolant) > UPDATE_THRESHOLD)
    {
        int32_t display_val = new_val / INT_SCALING; // e.g., 7243 / 100 = 72°F
        lv_meter_set_indicator_value(objects.coolant_temp, screen_main_state.coolant_temp, display_val);
        cached_coolant = new_val;
    }
}

void update_oil_pressure_meter(int32_t new_val)
{
    int32_t display_val = (new_val + 50) / 100; // e.g., 4500 -> 45 PSI

    if (abs(display_val - cached_oil) > UPDATE_THRESHOLD)
    {
        lv_meter_set_indicator_value(objects.oil_pressure, screen_main_state.oil_pressure, display_val);
        cached_oil = display_val;
    }
}

void update_fuel_arc(int32_t new_val)
{
    int32_t display_val = (new_val + 50) / 100; // e.g., 0-10000 -> 0-100 %

    if (abs(display_val - cached_fuel) > UPDATE_THRESHOLD)
    {
        lv_arc_set_value(objects.fuel_level, display_val);
        cached_fuel = display_val;
    }
}

void gauge_ui_update(bool is_stale)
{
    (void)is_stale;
    small_gauge_draw();
}