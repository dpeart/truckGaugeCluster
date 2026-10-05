#ifndef UPDATE_UI_H
#define UPDATE_UI_H

#include <stdint.h>
#include <stdbool.h>

#define UPDATE_THRESHOLD 0
#define INT_SCALING 100

#ifdef __cplusplus
extern "C" {
#endif

void update_iat_meter(int32_t new_val);
void update_egt_meter(int32_t new_val);
void update_battery_arc(int32_t new_val);
void gauge_ui_update(bool is_stale);

#ifdef __cplusplus
}
#endif

#endif // UPDATE_UI_H