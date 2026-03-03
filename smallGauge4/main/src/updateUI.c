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
