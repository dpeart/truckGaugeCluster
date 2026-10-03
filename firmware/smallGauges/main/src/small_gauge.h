#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "GaugePacket.h"

// Public API for small_gauge1
void small_gauge_init(void);
void small_gauge_deinit(void);

// Called from the UI update loop while LVGL is locked
void small_gauge_draw(void);

const GaugePacket *small_gauge_get_last_packet(void);

#ifdef __cplusplus
}
#endif
