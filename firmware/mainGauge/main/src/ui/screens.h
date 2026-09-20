#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_SETTINGS = 2,
    SCREEN_ID_STATS_SCREEN = 3,
    _SCREEN_ID_LAST = 3
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *settings;
    lv_obj_t *stats_screen;
    lv_obj_t *speed;
    lv_obj_t *tach;
    lv_obj_t *obj0;
    lv_obj_t *right;
    lv_obj_t *left;
    lv_obj_t *high_beam;
    lv_obj_t *odometer;
    lv_obj_t *config;
    lv_obj_t *config_button;
    lv_obj_t *obj1;
    lv_obj_t *p4_ota;
    lv_obj_t *obj2;
    lv_obj_t *p4_reboot;
    lv_obj_t *obj3;
    lv_obj_t *c6_ota;
    lv_obj_t *obj4;
    lv_obj_t *c6_reboot;
    lv_obj_t *obj5;
    lv_obj_t *c6_reset;
    lv_obj_t *stats_chart;
    lv_obj_t *distance_chart;
    lv_obj_t *speed_chart;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *quartermiletime;
    lv_obj_t *quartermilespeed;
    lv_obj_t *zero_to_sixty_time;
    lv_obj_t *stats_start;
    lv_obj_t *start;
} objects_t;

extern objects_t objects;

typedef struct {
    lv_meter_scale_t *scale;
    lv_meter_indicator_t *indicator;
    lv_meter_indicator_t *speed_indicator;
    lv_meter_scale_t *scale1;
    lv_meter_indicator_t *indicator1;
    lv_meter_indicator_t *tach_indicator;
} screen_main_state_t;

extern screen_main_state_t screen_main_state;

void create_screen_main();
void tick_screen_main();

void create_screen_settings();
void tick_screen_settings();

void create_screen_stats_screen();
void tick_screen_stats_screen();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/