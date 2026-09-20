#ifndef UPDATE_UI_H
#define UPDATE_UI_H

#include "lvgl.h"
#include "screens.h"

#define UPDATE_THRESHOLD 1

static inline float lerp(float current, float target, float factor) {
    return current + factor * (target - current);
}

// Prototypes for smallgauge2
void update_iat_meter(int32_t new_val);
void update_egt_meter(int32_t new_val);
void update_battery_arc(int32_t new_val);

#endif