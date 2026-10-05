#include "espnow_receiver.h"
#include "vcan_receiver.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"

static const char *TAG = "ESP-NOW-RECV";

/* ESP-NOW receive callback
   Forwards raw frames directly to vcan::Receiver for PGN parsing/dispatch. */
extern "C" void onReceive(const esp_now_recv_info_t *info,
                          const uint8_t *data, int len)
{
    if (!data || len <= 0) {
        ESP_LOGW(TAG, "espnow rx: empty packet");
        return;
    }

    // Direct C++ Singleton call to process the incoming frame
    esp_err_t err = vcan::Receiver::instance().processFrame(data, static_cast<size_t>(len));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "vcan::Receiver::processFrame failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "vcan::Receiver::processFrame success");
    }
}

extern "C" esp_err_t espnow_receiver_init(uint8_t channel)
{
    // Initialize ESP-NOW and register receive callback
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onReceive));

    ESP_LOGI(TAG, "espnow_receiver initialized on channel %u", channel);
    return ESP_OK;
}