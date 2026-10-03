#ifndef UPDATE_UI_H
#define UPDATE_UI_H

#include "lvgl.h"
#include "screens.h"
#include "small_Gauge.h"

#define UPDATE_THRESHOLD 1

#define INT_SCALING 100 // Scale down from backend resolution (scaled by 10) to match meter range

// Low-level meter handlers
void update_oil_temp_meter(int32_t new_val);
void update_boost_pressure_meter(int32_t new_val);

// High-level entry point for gauge_task
void gauge_ui_update(bool is_stale);

#endif // UPDATE_UI_H