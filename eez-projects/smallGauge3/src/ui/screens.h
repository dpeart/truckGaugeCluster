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
    _SCREEN_ID_LAST = 2
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *settings;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *trans_temp;
    lv_obj_t *fuel_pressure;
    lv_obj_t *obj2;
    lv_obj_t *config;
    lv_obj_t *config_button;
    lv_obj_t *main_button;
    lv_obj_t *obj3;
    lv_obj_t *ota;
    lv_obj_t *obj4;
    lv_obj_t *reboot;
    lv_obj_t *obj5;
    lv_obj_t *reset;
} objects_t;

extern objects_t objects;

typedef struct {
    lv_meter_scale_t *scale;
    lv_meter_indicator_t *indicator;
    lv_meter_indicator_t *trans_temp;
    lv_meter_scale_t *scale1;
    lv_meter_indicator_t *indicator1;
    lv_meter_indicator_t *fuel_pressure;
} screen_main_state_t;

extern screen_main_state_t screen_main_state;

void create_screen_main();
void tick_screen_main();

void create_screen_settings();
void tick_screen_settings();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/