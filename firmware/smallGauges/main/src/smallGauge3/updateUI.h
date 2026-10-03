#ifndef UPDATE_UI_H
#define UPDATE_UI_H


#include "lvgl.h"
#include "screens.h"
#include "GaugePacket.h"

#define UPDATE_THRESHOLD 1

#define INT_SCALING 100

// Low-level meter handlers
void update_trans_temp_meter(int32_t new_val);
void update_fuel_pressure_meter(int32_t new_val);

// High-level entry point for gauge_task
void gauge_ui_update(bool is_stale);

#endif // UPDATE_UI_H