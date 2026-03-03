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

void text_update_cb(lv_obj_t *label, const char *str)
{
    if(label == NULL || str == NULL) return;
    
    // Set the text
    lv_label_set_text(label, str);
}

void update_indicator_state(lv_obj_t *obj, bool active)
{
    if (obj == NULL) return;

    // LV_OPA_COVER (255) for ON, 50 for OFF
    lv_obj_set_style_img_opa(obj, active ? LV_OPA_COVER : 0, LV_PART_MAIN);
}

void updateIndicators(const GaugePacket *pkt)
{
    if (pkt == NULL) return;

    // 1. Map containers to their trigger conditions
    typedef struct {
        lv_obj_t *obj;
        bool active;
    } indicator_map_t;

    indicator_map_t map[] = {
        {objects.water_in_fuel,    (pkt->digitalPins & IND_WIF)},
        {objects.low_washer_fluid, (pkt->digitalPins & IND_WASHER)},
        {objects.low_fuel,         (pkt->fuelLevel < 0.125f)},
        {objects.low_battery,      (pkt->batteryLevel < 10.0f)},
        {objects.engine_temp,      (pkt->coolantTemp > 240.0f)}
    };
    int total_types = sizeof(map) / sizeof(map[0]);

    // 2. Identify active indicators
    int active_indices[total_types];
    int active_count = 0;

    for (int i = 0; i < total_types; i++) {
        if (map[i].obj != NULL && map[i].active) {
            active_indices[active_count++] = i;
        }
    }

    // 3. Handle 5-second rotation timing
    static int rotation_idx = 0;
    static int64_t last_switch = 0;
    int64_t now = esp_timer_get_time();

    if (now - last_switch > 5000000) { // 5 seconds
        rotation_idx++;
        last_switch = now;
    }

    // Ensure index wraps and resets if count changes
    if (active_count > 0) {
        rotation_idx %= active_count;
    } else {
        rotation_idx = 0;
    }

    // 4. Update Visibility
    // Hide ALL individual indicator containers
    for (int i = 0; i < total_types; i++) {
        if (map[i].obj != NULL) {
            lv_obj_add_flag(map[i].obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (active_count > 0) {
        // Show only the current one in rotation
        int target = active_indices[rotation_idx];
        lv_obj_clear_flag(map[target].obj, LV_OBJ_FLAG_HIDDEN);
        
        // Hide the Info container because a warning is active
        if (objects.info != NULL) {
            lv_obj_add_flag(objects.info, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        // No warnings active: Restore the Info container
        if (objects.info != NULL) {
            lv_obj_clear_flag(objects.info, LV_OBJ_FLAG_HIDDEN);
        }
    }
}