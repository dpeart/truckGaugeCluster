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

screen_main_state_t screen_main_state;

//
// Event handlers
//

lv_obj_t *tick_value_change_obj;

//
// Screens
//

void create_screen_main() {
    screen_main_state_t *state = &screen_main_state;
    (void)state;
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 800, 800);
    lv_obj_set_style_bg_img_src(obj, &img_main_gauge_bg, LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // speed
            lv_obj_t *obj = lv_meter_create(parent_obj);
            objects.speed = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 800, 800);
            {
                lv_meter_scale_t *scale = lv_meter_add_scale(obj);
                state->scale = scale;
                lv_meter_set_scale_ticks(obj, scale, 2, 0, 10, lv_color_hex(0xffffffff));
                lv_meter_set_scale_major_ticks(obj, scale, 10, 0, 20, lv_color_hex(0xffffffff), 30);
                lv_meter_set_scale_range(obj, scale, 0, 1410, 280, 130);
                {
                    lv_meter_indicator_t *indicator = lv_meter_add_arc(obj, scale, 0, lv_color_hex(0xffffffff), 0);
                    state->indicator = indicator;
                    lv_meter_set_indicator_start_value(obj, indicator, 0);
                    lv_meter_set_indicator_end_value(obj, indicator, 140);
                }
                {
                    lv_meter_indicator_t *indicator = lv_meter_add_needle_line(obj, scale, 3, lv_color_hex(0xffff7a00), -23);
                    state->speed_indicator = indicator;
                    lv_meter_set_indicator_value(obj, indicator, 0);
                }
            }
            lv_obj_set_style_text_font(obj, &ui_font_roboto_mono_40, LV_PART_TICKS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_TICKS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(obj, 0, LV_PART_TICKS | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // tach
            lv_obj_t *obj = lv_meter_create(parent_obj);
            objects.tach = obj;
            lv_obj_set_pos(obj, 198, 198);
            lv_obj_set_size(obj, 400, 400);
            {
                lv_meter_scale_t *scale = lv_meter_add_scale(obj);
                state->scale1 = scale;
                lv_meter_set_scale_ticks(obj, scale, 2, 0, 10, lv_color_hex(0xffffffff));
                lv_meter_set_scale_major_ticks(obj, scale, 5, 0, 20, lv_color_hex(0xffffffff), 20);
                lv_meter_set_scale_range(obj, scale, 0, 5000, 280, 130);
                {
                    lv_meter_indicator_t *indicator = lv_meter_add_arc(obj, scale, 0, lv_color_hex(0xffffffff), 0);
                    state->indicator1 = indicator;
                    lv_meter_set_indicator_start_value(obj, indicator, 0);
                    lv_meter_set_indicator_end_value(obj, indicator, 50);
                }
                {
                    lv_meter_indicator_t *indicator = lv_meter_add_needle_line(obj, scale, 3, lv_color_hex(0xffff7a00), -23);
                    state->tach_indicator = indicator;
                    lv_meter_set_indicator_value(obj, indicator, 0);
                }
            }
            lv_obj_set_style_text_font(obj, &ui_font_roboto_mono_30, LV_PART_TICKS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_TICKS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(obj, 0, LV_PART_TICKS | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        {
            // right
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.right = obj;
            lv_obj_set_pos(obj, 529, 694);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_right);
            lv_img_set_zoom(obj, 364);
            lv_obj_set_style_img_opa(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // left
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.left = obj;
            lv_obj_set_pos(obj, 238, 694);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_left);
            lv_img_set_zoom(obj, 364);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_opa(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // High Beam
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.high_beam = obj;
            lv_obj_set_pos(obj, 348, 660);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_high_beam);
            lv_img_set_zoom(obj, 135);
            lv_obj_set_style_img_opa(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // Odometer
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.odometer = obj;
            lv_obj_set_pos(obj, 334, 628);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_roboto_mono_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_letter_space(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "0002306.7");
        }
        {
            // CenterCap
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.center_cap = obj;
            lv_obj_set_pos(obj, 320, 320);
            lv_obj_set_size(obj, 160, 160);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_img_src(obj, &img_center, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
    screen_main_state_t *state = &screen_main_state;
    (void)state;
}

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

//
// Fonts
//

ext_font_desc_t fonts[] = {
    { "IBMPlexMono-Medium_24", &ui_font_ibm_plex_mono_medium_24 },
    { "IBMPlexMono_medium_30", &ui_font_ibm_plex_mono_medium_30 },
    { "IBMPlexMono-medium_35", &ui_font_ibm_plex_mono_medium_35 },
    { "Roboto-Mono-24", &ui_font_roboto_mono_24 },
    { "Roboto-Mono-30", &ui_font_roboto_mono_30 },
    { "Roboto-Mono-35", &ui_font_roboto_mono_35 },
    { "Roboto-Mono-40", &ui_font_roboto_mono_40 },
#if LV_FONT_MONTSERRAT_8
    { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
    { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
    { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
    { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
    { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
    { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
    { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
    { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
    { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
    { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
    { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
    { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
    { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
    { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
    { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
    { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
    { "MONTSERRAT_40", &lv_font_montserrat_40 },
#endif
#if LV_FONT_MONTSERRAT_42
    { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
    { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
    { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
    { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

uint32_t active_theme_index = 0;

//
//
//

void create_screens() {

// Set default LVGL theme
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    // Initialize screens
    // Create screens
    create_screen_main();
}