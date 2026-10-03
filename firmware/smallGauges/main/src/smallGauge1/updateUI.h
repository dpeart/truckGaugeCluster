#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Threshold used by update_* helpers to avoid excessive LVGL updates.
// If your project defines UPDATE_THRESHOLD elsewhere, that value will be used.
#ifndef UPDATE_THRESHOLD
#define UPDATE_THRESHOLD 2
#endif

 #define INT_SCALING 100 // scale factor for integer values (e.g., 7243 -> 72.43°F)

// UI update helpers used by gauge modules and the main UI dispatcher.
// Implementations live in updateUI.c.
void update_coolant_meter(int32_t new_val);
void update_oil_pressure_meter(int32_t new_val);
void update_fuel_arc(int32_t new_val);

// Single entry point called by gauge_task each frame.
// - is_stale indicates whether the telemetry link is considered stale.
// - pkt parameter removed: per-gauge modules read their own subscribed state.
void gauge_ui_update(bool is_stale);

#ifdef __cplusplus
}
#endif
