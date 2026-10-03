#pragma once

#include "GaugePacket.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize ESP-NOW receiver
esp_err_t espnow_receiver_init(void);

// Latest received GaugePacket (updated by ISR)
extern volatile GaugePacket g_latest_gauge;

#ifdef __cplusplus
}
#endif
