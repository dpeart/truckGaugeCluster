#include "espnow_receiver.h"
#include "GaugePacket.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"

#include "driver/uart.h"

#include <string.h>

static const char *TAG = "ESP-NOW-RECV";

// Latest received packet
static GaugePacket currentPacket;
static bool newPacket = false;

// UART settings
#define UART_PORT UART_NUM_2
#define UART_TX_PIN 17
#define UART_RX_PIN -1
#define UART_BAUD 921600

// -----------------------------------------------------------------------------
// UART forwarding using classic ESP-IDF UART driver (works in 5.5.2)
// -----------------------------------------------------------------------------
static esp_err_t uart_forward_init(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 2048, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "UART initialized (TX=%d, baud=%d)", UART_TX_PIN, UART_BAUD);
    return ESP_OK;
}

static void forwardToP4(const GaugePacket *pkt)
{
    const uint8_t START_BYTE = 0xAA;
    const uint8_t END_BYTE = 0x55;

    uart_write_bytes(UART_PORT, (const char *)&START_BYTE, 1);
    uart_write_bytes(UART_PORT, (const char *)pkt, sizeof(GaugePacket));
    uart_write_bytes(UART_PORT, (const char *)&END_BYTE, 1);
}

// -----------------------------------------------------------------------------
// ESP-NOW receive callback
// -----------------------------------------------------------------------------
static void onReceive(const esp_now_recv_info_t *info,
                      const uint8_t *data, int len)
{
    if (len != sizeof(GaugePacket))
    {
        ESP_LOGW(TAG, "Bad packet size: %d", len);
        return;
    }

    GaugePacket pkt;
    memcpy(&pkt, data, sizeof(GaugePacket));

    // printGaugePacket(&pkt);   // <-- NOW YOU PRINT THE REAL PACKET
    // Update the global gauge state used by gauge_task()
    gauge_state_set(&pkt); // <-- THIS LINE WAS MISSING

    memcpy(&currentPacket, &pkt, sizeof(GaugePacket));
    newPacket = true;

    forwardToP4(&currentPacket);
}

// -----------------------------------------------------------------------------
// Public API for gauge_task
// -----------------------------------------------------------------------------
// bool espnow_receiver_get(GaugePacket *out)
// {
//     if (!newPacket)
//         return false;
//     memcpy(out, &currentPacket, sizeof(GaugePacket));
//     newPacket = false;
//     return true;
// }

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------
esp_err_t espnow_receiver_init(uint8_t channel)
{
    // Initialize Wi-Fi radio only
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onReceive));

    return ESP_OK;
}
