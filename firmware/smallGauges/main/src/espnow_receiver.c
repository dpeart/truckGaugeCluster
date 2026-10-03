// espnow_receiver.c
#include "espnow_receiver.h"
#include "vcan_receiver.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "ESP-NOW-RECV";

/* ESP-NOW receive callback
   Forwards raw frames into vcan_receiver_process_frame for PGN parsing/dispatch.
   Keep this callback short and nonblocking. */
static void onReceive(const esp_now_recv_info_t *info,
                      const uint8_t *data, int len)
{
    if (!data || len <= 0) {
        ESP_LOGW(TAG, "espnow rx: empty packet");
        return;
    }

    // Print the sender's MAC address and packet length
    if (info && info->src_addr) {
        ESP_LOGI(TAG, "RX Packet from %02X:%02X:%02X:%02X:%02X:%02X | Len: %d",
                 info->src_addr[0], info->src_addr[1], info->src_addr[2],
                 info->src_addr[3], info->src_addr[4], info->src_addr[5], len);
    } else {
        ESP_LOGI(TAG, "RX Packet (Unknown Source) | Len: %d", len);
    }

    // Dump raw payload bytes to verify they match the sender's hex logs
    ESP_LOG_BUFFER_HEX(TAG, data, len);

    // Forward raw frame into vcan receiver (should validate/enqueue internally)
    esp_err_t err = vcan_receiver_process_frame(data, (size_t)len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "vcan_receiver_process_frame failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "vcan_receiver_process_frame success");
    }
}

esp_err_t espnow_receiver_init(uint8_t channel)
{
    // WiFi channel selection is handled by the main WiFi state machine.

    // Initialize ESP-NOW and register receive callback
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onReceive));

    ESP_LOGI(TAG, "espnow_receiver initialized on channel %u", channel);
    return ESP_OK;
}
