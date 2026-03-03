#ifndef UPDATE_SPEED_H
#define UPDATE_SPEED_H

#include <stdint.h>
#include "lvgl.h"
#include "src/ui/screens.h" // Access to 'objects' and UI pointers
#include "GaugePacket.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Animates the 'speed' meter needle smoothly.
     * @param target_speed The new speed value to animate to.
     */
    // void update_coolant_ui(int32_t current_speed, int32_t target_speed);
    // void update_oil_pressure_ui(int32_t current_rpm, int32_t target_rpm);

    extern volatile bool ui_ready;
    extern volatile bool lvgl_started;
    
    void meter_anim_cb(lv_obj_t *meter, lv_meter_indicator_t *ind, int32_t val);
    void arc_anim_cb(lv_obj_t *arc, int32_t val);

#ifdef __cplusplus
}
#endif

#endif
