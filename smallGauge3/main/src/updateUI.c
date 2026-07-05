#include "updateUI.h"
#include <math.h>

// Static state tracking
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