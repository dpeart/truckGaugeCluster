// updateUI.c
#include "updateUI.h"
#include "small_gauge.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "updateUI";

static int16_t s_cached_ambient = -999;
static char    s_cached_heading[16] = {0};
static char    s_cached_time[10] = {0};

void update_ambient_temp_display(int16_t ambient_temp)
{
    if (ambient_temp != s_cached_ambient) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d", ambient_temp / INT_SCALING);
        
        if (objects.ambient_temp) {
            lv_label_set_text(objects.ambient_temp, buf);
        }
        s_cached_ambient = ambient_temp;
    }
}

void update_heading_display(const char *heading_str)
{
    if (!heading_str) return;

    if (strcmp(heading_str, s_cached_heading) != 0) {
        if (objects.heading) {
            lv_label_set_text(objects.heading, heading_str);
        }
        strncpy(s_cached_heading, heading_str, sizeof(s_cached_heading) - 1);
        s_cached_heading[sizeof(s_cached_heading) - 1] = '\0';
    }
}

void update_time_display(uint8_t hour, uint8_t minute)
{
    char current_time[10];
    int display_hour = hour % 12;
    if (display_hour == 0) display_hour = 12;

    snprintf(current_time, sizeof(current_time), "%d:%02d", display_hour, minute);

    if (strcmp(current_time, s_cached_time) != 0) {
        if (objects.time) {
            lv_label_set_text(objects.time, current_time);
        }
        strncpy(s_cached_time, current_time, sizeof(s_cached_time) - 1);
        s_cached_time[sizeof(s_cached_time) - 1] = '\0';
    }
}

void update_indicators_display(bool wif, bool washer, bool low_fuel, bool low_batt, bool eng_temp)
{
    typedef struct {
        lv_obj_t **obj;
        bool active;
        const char *name;
    } indicator_map_t;

    indicator_map_t map[] = {
        {&objects.water_in_fuel,    wif,      "Water-In-Fuel"},
        {&objects.low_washer_fluid, washer,   "Low-Washer"},
        {&objects.low_fuel,         low_fuel, "Low-Fuel"},
        {&objects.low_battery,      low_batt, "Low-Battery"},
        {&objects.engine_temp,      eng_temp, "Engine-Temp"}
    };

    const int total = sizeof(map) / sizeof(map[0]);
    int active_indices[5];
    int active_count = 0;

    for (int i = 0; i < total; i++) {
        if (map[i].active && map[i].obj && *map[i].obj) {
            active_indices[active_count++] = i;
        }
    }

    static int rotation_idx = 0;
    static int prev_active_count = 0;
    static int64_t last_switch = 0;
    int64_t now = esp_timer_get_time();

    // Reset rotation timer/index if active set changes size
    if (active_count != prev_active_count) {
        rotation_idx = 0;
        last_switch = now;
        prev_active_count = active_count;
    } else if (active_count > 1 && (now - last_switch >= 5000000LL)) { 
        // 5-second interval rotation across multiple active warnings
        rotation_idx = (rotation_idx + 1) % active_count;
        last_switch = now;
    } else if (active_count > 0) {
        rotation_idx %= active_count;
    }

    // Hide all indicator icons
    for (int i = 0; i < total; i++) {
        if (map[i].obj && *map[i].obj) {
            lv_obj_add_flag(*map[i].obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (active_count > 0) {
        int target = active_indices[rotation_idx];

        if (map[target].obj && *map[target].obj) {
            lv_obj_clear_flag(*map[target].obj, LV_OBJ_FLAG_HIDDEN);
        }
        if (objects.info) {
            lv_obj_add_flag(objects.info, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        // Fallback to primary info widget when no alerts active
        if (objects.info) {
            lv_obj_clear_flag(objects.info, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void gauge_ui_update(bool is_stale)
{
    (void)is_stale;
    small_gauge_draw();
}