#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;

static lv_meter_scale_t * scale0;
static lv_meter_indicator_t * indicator1;
static lv_meter_scale_t * scale2;
static lv_meter_indicator_t * indicator3;

void create_screen_main() {
    void *flowState = getFlowState(0, 0);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 720, 720);
    {
        lv_obj_t *parent_obj = obj;
        {
            // speed
            lv_obj_t *obj = lv_meter_create(parent_obj);
            objects.speed = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 720, 720);
            {
                lv_meter_scale_t *scale = lv_meter_add_scale(obj);
                scale0 = scale;
                lv_meter_set_scale_ticks(obj, scale, 141, 2, 10, lv_color_hex(0xffffffff));
                lv_meter_set_scale_major_ticks(obj, scale, 10, 5, 20, lv_color_hex(0xffffffff), 25);
                lv_meter_set_scale_range(obj, scale, 0, 140, 280, 130);
                {
                    lv_meter_indicator_t *indicator = lv_meter_add_needle_line(obj, scale, 3, lv_color_hex(0xff0000ff), -28);
                    indicator1 = indicator;
                    lv_meter_set_indicator_value(obj, indicator, 0);
                }
            }
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_34, LV_PART_TICKS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_TICKS | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_arc_create(parent_obj);
                    objects.obj0 = obj;
                    lv_obj_set_pos(obj, -2, -2);
                    lv_obj_set_size(obj, 680, 680);
                    lv_arc_set_range(obj, 0, 0);
                    lv_arc_set_value(obj, 25);
                    lv_arc_set_bg_start_angle(obj, 130);
                    lv_arc_set_bg_end_angle(obj, 50);
                    lv_obj_set_style_arc_width(obj, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_width(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
                }
                {
                    // tach
                    lv_obj_t *obj = lv_meter_create(parent_obj);
                    objects.tach = obj;
                    lv_obj_set_pos(obj, 158, 158);
                    lv_obj_set_size(obj, 360, 360);
                    {
                        lv_meter_scale_t *scale = lv_meter_add_scale(obj);
                        scale2 = scale;
                        lv_meter_set_scale_ticks(obj, scale, 51, 0, 10, lv_color_hex(0xffffffff));
                        lv_meter_set_scale_major_ticks(obj, scale, 5, 3, 20, lv_color_hex(0xffffffff), 25);
                        lv_meter_set_scale_range(obj, scale, 0, 50, 280, 130);
                        {
                            lv_meter_indicator_t *indicator = lv_meter_add_needle_line(obj, scale, 3, lv_color_hex(0xff0000ff), -28);
                            indicator3 = indicator;
                            lv_meter_set_indicator_value(obj, indicator, 0);
                        }
                    }
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_TICKS | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_TICKS | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_arc_create(parent_obj);
                            objects.obj1 = obj;
                            lv_obj_set_pos(obj, -2, -2);
                            lv_obj_set_size(obj, 320, 320);
                            lv_arc_set_range(obj, 0, 0);
                            lv_arc_set_value(obj, 25);
                            lv_arc_set_bg_start_angle(obj, 130);
                            lv_arc_set_bg_end_angle(obj, 50);
                            lv_obj_set_style_arc_width(obj, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_width(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_opa(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
                        }
                    }
                }
            }
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
    void *flowState = getFlowState(0, 0);
    (void)flowState;
}


static const char *screen_names[] = { "Main" };
static const char *object_names[] = { "main", "speed", "obj0", "tach", "obj1" };


typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    eez_flow_init_screen_names(screen_names, sizeof(screen_names) / sizeof(const char *));
    eez_flow_init_object_names(object_names, sizeof(object_names) / sizeof(const char *));
    
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main();
}
