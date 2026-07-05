#include "updateUI.h"
#include <math.h>

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