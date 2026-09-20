#ifndef UPDATE_UI_H
#define UPDATE_UI_H

#include "lvgl.h"
#include "screens.h"
#include "GaugePacket.h"

#define UPDATE_THRESHOLD 1

static inline float lerp(float current, float target, float factor) {
    return current + factor * (target - current);
}

// Prototypes for optimized UI updates
void update_oil_temp_meter(int32_t new_val);
void update_boost_pressure_meter(int32_t new_val);

#endif