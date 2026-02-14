#include "espnow_task.h"
#include "espnow_receiver.h"
#include "GaugePacket.h"
#include "debug.h"
#include "esp_log.h"

static const char* TAG = "ESP-NOW-TASK";

static void espnow_task(void* arg)
{
    ESP_LOGI(TAG, "ESP-NOW task started");

    GaugePacket pkt{};

    for (;;)
    {
        // Block until a packet arrives
        if (xQueueReceive(espnow_rx_queue, &pkt, portMAX_DELAY) == pdTRUE)
        {
            // Push into global gauge state
            gauge_state_set(pkt);

            // Optional debug
            ESP_LOGI(TAG, "Packet: speed=%d rpm=%d", pkt.speed, pkt.rpm);
            // printGaugePacket(pkt); // if you want full dump
        }
    }
}

void start_espnow_task(UBaseType_t core)
{
    constexpr uint32_t STACK_SIZE = 4096;
    constexpr UBaseType_t PRIORITY = 5;

    xTaskCreatePinnedToCore(
        espnow_task,
        "espnow_task",
        STACK_SIZE,
        nullptr,
        PRIORITY,
        nullptr,
        core
    );
}
