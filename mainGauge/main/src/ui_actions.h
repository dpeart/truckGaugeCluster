#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool ota_upload_in_progress;

void action_switch_screen(lv_event_t *e);
void action_button_pressed(lv_event_t *e);
void action_swipe_screen(lv_event_t *e);

#ifdef __cplusplus
}
#endif