#include "updateUI.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "digitalPins.h"
#include "GaugePacket.h"


volatile bool ui_ready = false;
volatile bool lvgl_started = false;

void meter_anim_cb(lv_obj_t *meter, lv_meter_indicator_t *ind, int32_t val)
{
    lv_meter_set_indicator_value(meter, ind, val);
}

void arc_anim_cb(lv_obj_t *arc, int32_t val)
{
    lv_arc_set_value(arc, val);
}

// // Callback for the LVGL animation engine
// void coolant_anim_cb(void *var, int32_t val)
// {
//     // Uses the 'speed' object from screens.h
//     lv_meter_set_indicator_value(objects.coolant_temp, (lv_meter_indicator_t *)var, val);
//     // ESP_LOGI("tag", "%d", val);
// }

// void oil_pressure_anim_cb(void *var, int32_t val)
// {

//     // Update the indicator
//     lv_meter_set_indicator_value(objects.oil_pressure, (lv_meter_indicator_t *)var, val);
// }

// void fuel_level_anim_cb(void *var, int32_t val)
// {

//     // Update the indicator
//     lv_arc_set_value(objects.fuel_level, val);
// }
