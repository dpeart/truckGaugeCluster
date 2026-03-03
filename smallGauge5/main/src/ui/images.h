#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_battery_warning_light;
extern const lv_img_dsc_t img_engine_temp_icon;
extern const lv_img_dsc_t img_fuel_location_right;
extern const lv_img_dsc_t img_low_washer_fluid;
extern const lv_img_dsc_t img_od_off_indicator;
extern const lv_img_dsc_t img_oil_pressure_indicator;
extern const lv_img_dsc_t img_water_in_fuel;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[7];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/