#include "updateUI.h"
#include <math.h>

static int32_t cached_oil = -999;
static int32_t cached_boost = -999;

void update_oil_temp_meter(int32_t new_val) {
    if (abs(new_val - cached_oil) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.oil_temp, screen_main_state.oil_temp, new_val);
        cached_oil = new_val;
    }
}

void update_boost_pressure_meter(int32_t new_val) {
    if (abs(new_val - cached_boost) > UPDATE_THRESHOLD) {
        lv_meter_set_indicator_value(objects.boost_pressure, screen_main_state.boost_pressure, new_val);
        cached_boost = new_val;
    }
}