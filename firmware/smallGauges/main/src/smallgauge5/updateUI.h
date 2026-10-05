#ifndef UPDATE_UI_H
#define UPDATE_UI_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "screens.h" // Access to 'objects' and UI pointers

#ifdef __cplusplus
extern "C"
{
#endif

    #define INT_SCALING 100 // Scale down from backend resolution

    extern volatile bool ui_ready;
    extern volatile bool lvgl_started;

    void text_update_cb(lv_obj_t *label, const char *str);

    /* UI update functions for small_gauge_draw() */
    void update_ambient_temp_display(int16_t ambient_temp);
    void update_heading_display(const char *heading_str);
    void update_time_display(uint8_t hour, uint8_t minute);
    void update_indicators_display(bool wif, bool washer, bool low_fuel, bool low_batt, bool eng_temp);

    // High-level entry point for gauge_task
    void gauge_ui_update(bool is_stale);

#ifdef __cplusplus
}
#endif

#endif // UPDATE_UI_H
