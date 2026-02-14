#pragma once

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Our types
#include "GaugePacket.h"

// Start the ESP-NOW processing task
void start_espnow_task(UBaseType_t core = tskNO_AFFINITY);
