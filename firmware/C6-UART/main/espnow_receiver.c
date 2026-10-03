#include "espnow_receiver.h"
#include "c6_modes.h"
#include "c6_uart.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "C6_ESPNOW_RX";

static void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (!data || len <= 0) {
        ESP_LOGW(TAG, "Empty ESP-NOW payload");
        return;
    }

    // Only forward telemetry when C6 is in TELEMETRY mode
    if (current_mode == MODE_TELEMETRY) {
        // Forward raw VCAN frame immediately over UART to P4
        uart_send_vcan_frame(data, (uint16_t)len);
    }
}

esp_err_t espnow_receiver_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW VCAN receiver");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_now_register_recv_cb(onReceive);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_register_recv_cb failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ESP-NOW VCAN receiver initialized");
    return ESP_OK;
}