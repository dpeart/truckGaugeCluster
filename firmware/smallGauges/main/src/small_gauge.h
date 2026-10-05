#ifndef SMALL_GAUGE_H
#define SMALL_GAUGE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the small gauge module, register callbacks with vcan::Receiver,
 *        and prime initial state values.
 */
void small_gauge_init(void);

/**
 * @brief Unregister callbacks and free local mutex resources.
 */
void small_gauge_deinit(void);

/**
 * @brief Performs local smoothing and updates UI meters/arcs.
 *        Must be called with the LVGL mutex held.
 */
void small_gauge_update(void);

/**
 * @brief Draw the small gauge UI elements.
 *        Must be called with the LVGL mutex held.
 */
void small_gauge_draw(void);

#ifdef __cplusplus
}
#endif

#endif // SMALL_GAUGE_H