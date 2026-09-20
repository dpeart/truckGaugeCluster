#include "updateUI.h"
#include <math.h>

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