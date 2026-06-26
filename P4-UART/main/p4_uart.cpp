#include <string.h>
#include "p4_uart.h"
#include "p4_modes.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "GaugePacket.h"
#include "p4_telemetry.h"
#include "p4_ota.h"
#include "esp_ota_ops.h"

static const char *TAG = "P4_UART";

#define UART_PORT UART_NUM_1
#define UART_TX_PIN GPIO_NUM_14
#define UART_RX_PIN GPIO_NUM_15
#define UART_BAUD 115200

#define FRAME_START 0xAA
#define ESCAPE 0x7D
#define MAX_PAYLOAD 256

uint8_t calc_crc(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++)
        crc ^= data[i];
    return crc;
}

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

// [0] FRAME_START
// [1] CMD
// [2] LEN_L
// [3] LEN_H
// [4..] PAYLOAD
// [last] CRC

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

static void handle_command(uint8_t cmd, uint8_t *payload, uint16_t len)
{
    switch (cmd)
    {
    case CMD_PING:
        uart_send_frame(CMD_PONG, NULL, 0);
        break;

    case CMD_BOOTED:
        ESP_LOGI(TAG, "C6 reports BOOTED");
        break;

    case CMD_STREAM_GAUGE:
    {
        if (len != sizeof(GaugePacket))
        {
            ESP_LOGW(TAG, "Bad GaugePacket size: %u (expected %u)",
                     len, (unsigned)sizeof(GaugePacket));
            break;
        }

        GaugePacket pkt;
        memcpy(&pkt, payload, sizeof(GaugePacket));
        handle_gauge_packet(&pkt);
        break;
    }

    case CMD_ACK:
        handle_ack(payload, len);
        break;

    case CMD_STREAM_FIRMWARE:
        p4_ota_queue_chunk(payload, len); // ✔ queue it
        uart_send_frame(CMD_ACK, NULL, 0);
        break;

    case CMD_STREAM_FIRMWARE_DONE:
    {
        ESP_LOGI(TAG, "Received OTA DONE from C6");
        p4_ota_finalize_event(); // queue it

        break;
    }

    case CMD_MODE_P4_OTA:
        current_mode = p4_mode_t::P4_OTA;
        break;

    case CMD_MODE_TELEMETRY:
        current_mode = p4_mode_t::TELEMETRY;
        break;

    default:
        ESP_LOGW(TAG, "Unknown CMD: 0x%02X", cmd);
        break;
    }
}

static void uart_rx_task(void *arg)
{
    uint8_t buf[256];
    uint8_t payload[256];
    int state = -1; // -1 = waiting for FRAME_START
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
            case 0: // CMD
                cmd = b;
                state = 1;
                break;

            case 1: // LEN_L
                len = b;
                state = 2;
                break;

            case 2: // LEN_H
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

            case 3: // PAYLOAD
                payload[payload_idx++] = b;

                if (payload_idx >= len)
                    state = 4;

                break;

            case 4: // CRC
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

void start_uart_rx_task(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = 0};

    uart_driver_install(UART_PORT, 4096, 4096, 0, NULL, 0);
    uart_param_config(UART_PORT, &cfg);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(uart_rx_task, "p4_uart_rx_task", 4096, NULL, 5, NULL);
}
