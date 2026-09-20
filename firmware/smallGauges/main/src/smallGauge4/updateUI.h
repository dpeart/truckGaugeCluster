#ifndef UPDATE_UI_H
#define UPDATE_UI_H

#include "lvgl.h"
#include "screens.h"
#include "GaugePacket.h"

#define UPDATE_THRESHOLD 1

// Low-level meter handlers
void update_oil_temp_meter(int32_t new_val);
void update_boost_pressure_meter(int32_t new_val);

// High-level entry point for gauge_task
void gauge_ui_update(const GaugePacket *pkt, bool is_stale);

#endif // UPDATE_UI_H