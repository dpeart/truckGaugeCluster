#ifndef UPDATE_SPEED_H
#define UPDATE_SPEED_H

#include <stdint.h>
#include "lvgl.h"
#include "src/ui/screens.h" // Access to 'objects' and UI pointers
#include "GaugePacket.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Animates the 'speed' meter needle smoothly.
     * @param target_speed The new speed value to animate to.
     */
    void update_coolant_ui(int32_t current_speed, int32_t target_speed);
    void update_oil_pressure_ui(int32_t current_rpm, int32_t target_rpm);

    void coolant_anim_cb(void *var, int32_t val);
    void oil_pressure_anim_cb(void *var, int32_t val);
    void fuel_level_anim_cb(void *var, int32_t val);

#ifdef __cplusplus
}
#endif

#endif
