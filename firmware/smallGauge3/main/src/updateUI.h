#ifndef UPDATE_UI_H
#define UPDATE_UI_H

#include "lvgl.h"
#include "screens.h"
#include "GaugePacket.h"

// Threshold to trigger an update (prevents jitter)
#define UPDATE_THRESHOLD 1

// LERP function
static inline float lerp(float current, float target, float factor) {
    return current + factor * (target - current);
}

// Prototypes for optimized UI updates
void update_trans_temp_meter(int32_t new_val);
void update_fuel_pressure_meter(int32_t new_val);

#endif