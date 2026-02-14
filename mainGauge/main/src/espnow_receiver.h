#pragma once

#include "GaugePacket.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_hosted.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

// Queue where ESP-NOW packets are delivered
extern QueueHandle_t espnow_rx_queue;

// Initialize ESP-NOW receiver on a given WiFi channel
esp_err_t espnow_receiver_init(uint8_t channel);

// Non-blocking: returns true if a packet was available
bool espnow_receiver_get(GaugePacket &out);
void espnow_receiver_pump();
