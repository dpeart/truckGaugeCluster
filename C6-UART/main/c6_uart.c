#include "c6_uart.h"
#include "c6_modes.h"
#include "GaugePacket.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "espnow_receiver.h"
#include "wifi_ota.h"
#include <string.h>

#define TAG "C6_UART"

#define UART_PORT UART_NUM_1
#define UART_TX_PIN GPIO_NUM_21
#define UART_RX_PIN GPIO_NUM_20
#define UART_BAUD 115200

#define FRAME_START 0xAA
#define ESCAPE 0x7D
#define MAX_PAYLOAD 256

static SemaphoreHandle_t ack_sem = NULL;
bool ota_upload_in_progress = false; // true = OTA upload in progress, false = OTA idle

// ---------------------------------------------------------
// CRC
// ---------------------------------------------------------
uint8_t calc_crc(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++)
        crc ^= data[i];
    return crc;
}

// ---------------------------------------------------------
// Escaping helper
// ---------------------------------------------------------
static inline void uart_put_byte(uint8_t b, uint8_t *buf, int *idx)
{
    if (b == FRAME_START || b == ESCAPE)
    {
        buf[(*idx)++] = ESCAPE;
        buf[(*idx)++] = (uint8_t)(b ^ 0x20);
    }
    else
    {
        buf[(*idx)++] = b;
    }
}

// ---------------------------------------------------------
// Send frame
// ---------------------------------------------------------
void uart_send_frame(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    if (len > MAX_PAYLOAD)
        len = MAX_PAYLOAD;

    uint8_t len_l = len & 0xFF;
    uint8_t len_h = (len >> 8) & 0xFF;

    uint8_t tmp[3 + MAX_PAYLOAD];
    tmp[0] = cmd;
    tmp[1] = len_l;
    tmp[2] = len_h;
    if (len > 0)
        memcpy(&tmp[3], payload, len);

    uint8_t crc = calc_crc(tmp, len + 3);

    uint8_t buf[4 + (MAX_PAYLOAD * 2) + 2];
    int idx = 0;

    buf[idx++] = FRAME_START;

    uart_put_byte(cmd, buf, &idx);
    uart_put_byte(len_l, buf, &idx);
    uart_put_byte(len_h, buf, &idx);

    for (int i = 0; i < len; i++)
        uart_put_byte(payload[i], buf, &idx);

    uart_put_byte(crc, buf, &idx);

    uart_write_bytes(UART_PORT, buf, idx);
}

// ---------------------------------------------------------
// ACK wait
// ---------------------------------------------------------
bool wait_for_ack(uint32_t timeout_ms)
{
    if (xSemaphoreTake(ack_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
        return true;

    return false;
}

// ---------------------------------------------------------
// Send GaugePacket
// ---------------------------------------------------------
void uart_send_gauge_packet(const GaugePacket *pkt)
{
    uart_send_frame(CMD_STREAM_GAUGE,
                    (const uint8_t *)pkt,
                    sizeof(GaugePacket));
}

// ---------------------------------------------------------
// OTA chunk sender (C6 → P4)
// ---------------------------------------------------------
void uart_send_firmware_chunk(const uint8_t *data, uint16_t len)
{
    while (len > 0)
    {
        uint16_t chunk_len = (len > 256) ? 256 : len;

        uart_send_frame(CMD_STREAM_FIRMWARE, data, chunk_len);

        if (!wait_for_ack(1000))
        {
            ESP_LOGE(TAG, "P4 did not ACK");
            return;
        }

        data += chunk_len;
        len -= chunk_len;
    }
}

// ---------------------------------------------------------
// RX command handler
// ---------------------------------------------------------
static void handle_command(uint8_t cmd, uint8_t *payload, uint16_t len)
{
    switch (cmd)
    {
    case CMD_ACK:
        xSemaphoreGive(ack_sem);
        break;

    case CMD_PING:
        ESP_LOGI(TAG, "Received PING, sending PONG");
        uart_send_frame(CMD_PONG, NULL, 0);
        break;

    case CMD_MODE_C6_OTA:
        current_mode = MODE_C6_OTA;
        break;

    case CMD_MODE_P4_OTA:
        current_mode = MODE_P4_OTA;
        break;

    case CMD_MODE_FACTORY_RESET:
        current_mode = MODE_FACTORY_RESET;
        break;

    case CMD_MODE_REBOOT:
        current_mode = MODE_REBOOT;
        break;

    case CMD_MODE_PROVISIONING:
        current_mode = MODE_PROVISIONING;
        break;

    case CMD_MODE_TELEMETRY:
        ESP_LOGW("C6_MODE", "Switching to TELEMETRY mode");

        // 1. Fully exit OTA Wi-Fi mode
        exit_ota_mode();

        // 2. Restore C6 internal mode
        current_mode = MODE_TELEMETRY;
        ota_upload_in_progress = false;

        break;

    case CMD_DUMMY_DATA:
        ESP_LOGI(TAG, "Telemetry: %02X %02X %02X",
                 payload[0], payload[1], payload[2]);
        break;

    default:
        ESP_LOGW(TAG, "Unknown CMD: 0x%02X", cmd);
        break;
    }
}

// ---------------------------------------------------------
// RX task (escaped protocol)
// ---------------------------------------------------------
static void uart_rx_task(void *arg)
{
    uint8_t buf[256];
    uint8_t payload[256];
    int state = -1;
    uint8_t cmd = 0;
    uint16_t len = 0;
    int payload_idx = 0;
    bool escape_next = false;

    while (true)
    {
        int n = uart_read_bytes(UART_PORT, buf, sizeof(buf), pdMS_TO_TICKS(20));
        if (n <= 0)
            continue;

        for (int i = 0; i < n; i++)
        {
            uint8_t b = buf[i];

            // Unescape
            if (escape_next)
            {
                b ^= 0x20;
                escape_next = false;
            }
            else if (b == ESCAPE)
            {
                escape_next = true;
                continue;
            }

            // Frame start
            if (b == FRAME_START && state == -1)
            {
                state = 0;
                len = 0;
                payload_idx = 0;
                continue;
            }

            if (state == -1)
                continue;

            switch (state)
            {
            case 0:
                cmd = b;
                state = 1;
                break;

            case 1:
                len = b;
                state = 2;
                break;

            case 2:
                len |= ((uint16_t)b << 8);

                if (len > sizeof(payload))
                {
                    ESP_LOGW(TAG, "Invalid length %u, dropping frame", len);
                    state = -1;
                    break;
                }

                payload_idx = 0;
                state = (len == 0) ? 4 : 3;
                break;

            case 3:
                payload[payload_idx++] = b;

                if (payload_idx >= len)
                    state = 4;

                break;

            case 4:
            {
                uint8_t tmp[3 + 256];
                tmp[0] = cmd;
                tmp[1] = (uint8_t)(len & 0xFF);
                tmp[2] = (uint8_t)(len >> 8);

                if (len > 0)
                    memcpy(&tmp[3], payload, len);

                if (calc_crc(tmp, len + 3) == b)
                    handle_command(cmd, payload, len);
                else
                    ESP_LOGW(TAG, "CRC error");

                state = -1;
                break;
            }
            }
        }
    }
}

// ---------------------------------------------------------
// Start UART
// ---------------------------------------------------------
void start_uart_rx_task(void)
{
    ack_sem = xSemaphoreCreateBinary();

    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT};

    uart_driver_install(UART_PORT, 4096, 4096, 0, NULL, 0);
    uart_param_config(UART_PORT, &cfg);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 5, NULL);
}
