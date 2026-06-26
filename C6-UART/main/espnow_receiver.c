#include "espnow_receiver.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "C6_ESPNOW";

// Latest received packet (ISR writes, telemetry task reads)
volatile GaugePacket g_latest_gauge;

static void onReceive(const esp_now_recv_info_t *info,
                      const uint8_t *data, int len)
{
    if (len != sizeof(GaugePacket)) {
        ESP_LOGW(TAG, "Bad packet size: %d", len);
        return;
    }

    // Copy into global buffer
    memcpy((void *)&g_latest_gauge, data, sizeof(GaugePacket));
}

esp_err_t espnow_receiver_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW receiver");

    // WiFi must be in STA mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Init ESP-NOW
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Register RX callback
    err = esp_now_register_recv_cb(onReceive);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_register_recv_cb failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ESP-NOW receiver initialized");
    return ESP_OK;
}
