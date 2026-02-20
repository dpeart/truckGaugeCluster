#pragma once

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "GaugePacket.h"

#ifdef __cplusplus
extern "C" {
#endif

// Queue where ESP-NOW packets are delivered (currently unused; we
// update the global gauge state directly in the RX callback).  If you
// revert to a queue-based model re-enable this handle and create the
// queue in espnow_receiver_init().
// extern QueueHandle_t espnow_rx_queue;

// Initialize ESP-NOW receiver on a given WiFi channel
esp_err_t espnow_receiver_init(uint8_t channel);

// Non-blocking: returns true if a packet was available
// bool espnow_receiver_get(GaugePacket *out);

// Pump function (called from LVGL task)
// void espnow_receiver_pump(void);

#ifdef __cplusplus
}
#endif