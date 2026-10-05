#include "vcan_sender.h"
#include "esp_log.h"
#include "esp_now.h"
#include <cstring>

static const char *TAG = "VCAN-SENDER";

namespace vcan {

Sender& Sender::instance() {
    static Sender instance;
    return instance;
}

esp_err_t Sender::init() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) return ESP_OK;

    // Perform any ESP-NOW hardware initialization / driver checks if needed
    ESP_LOGI(TAG, "vcan::Sender initialized");

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Sender::sendRawPayload(uint16_t pgn, uint8_t src_addr, uint8_t priority, const void *payload, uint8_t len) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        ESP_LOGE(TAG, "Sender not initialized!");
        return ESP_ERR_INVALID_STATE;
    }

    // Packet structure: Header + Payload
    uint8_t buffer[256];
    if (len + 4 > sizeof(buffer)) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Build CAN/vCAN frame header format
    buffer[0] = (uint8_t)(pgn >> 8);
    buffer[1] = (uint8_t)(pgn & 0xFF);
    buffer[2] = src_addr;
    buffer[3] = priority;

    if (payload && len > 0) {
        memcpy(&buffer[4], payload, len);
    }

    // Broadcast address (FF:FF:FF:FF:FF:FF)
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    esp_err_t err = esp_now_send(broadcast_mac, buffer, len + 4);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_send failed for PGN 0x%04X: %s", pgn, esp_err_to_name(err));
    }

    return err;
}

} // namespace vcan