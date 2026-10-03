#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void main_cluster_init(void);
void main_cluster_deinit(void);
void gauge_ui_update(bool is_stale);

// --- New Speed Accessor for StatsModule ---
float get_current_speed_mph(void);

#ifdef __cplusplus
}
#endif