#ifndef UPDATE_UI_H
#define UPDATE_UI_H

#include "lvgl.h"
#include "screens.h"

// Threshold to trigger an update (prevents jitter)
#define UPDATE_THRESHOLD 1

// LERP function
static inline float lerp(float current, float target, float factor) {
    return current + factor * (target - current);
}

void update_coolant_meter(int32_t new_val);
void update_oil_pressure_meter(int32_t new_val);
void update_fuel_arc(int32_t new_val);

#endif