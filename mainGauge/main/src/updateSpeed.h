#ifndef UPDATE_SPEED_H
#define UPDATE_SPEED_H

#include <stdint.h>
#include "lvgl.h"
#include "src/ui/screens.h" // Access to 'objects' and UI pointers

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Animates the 'speed' meter needle smoothly.
     * @param target_speed The new speed value to animate to.
     */
    void update_speed_ui(int32_t current_speed, int32_t target_speed);
    void update_tach_ui(int32_t current_rpm, int32_t target_rpm);

    void generateTurnSignalPattern();
    void incrementOdometer();

#ifdef __cplusplus
}
#endif

#endif
