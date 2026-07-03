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
#include "ui_actions.h"

static const char *TAG = "P4_UART";
bool ota_upload_in_progress = false;
extern QueueHandle_t ota_queue;

#define UART_PORT UART_NUM_1
#define UART_TX_PIN GPIO_NUM_14
#define UART_RX_PIN GPIO_NUM_15
#define UART_RTS_PIN GPIO_NUM_16
#define UART_CTS_PIN GPIO_NUM_17
#define UART_BAUD 460800

#define FRAME_MAGIC 0x54414F50 // ASCII "POAT" (P4 OTA)

uint8_t calc_crc(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++)
        crc ^= data[i];
    return crc;
}

// Synchronous Transmission Function
void uart_send_frame(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    if (len > MAX_PAYLOAD)
        len = MAX_PAYLOAD;

    uint32_t magic = FRAME_MAGIC;
    uint8_t header[7];

    header[0] = (uint8_t)(magic & 0xFF);
    header[1] = (uint8_t)((magic >> 8) & 0xFF);
    header[2] = (uint8_t)((magic >> 16) & 0xFF);
    header[3] = (uint8_t)((magic >> 24) & 0xFF);
    header[4] = cmd;
    header[5] = (uint8_t)(len & 0xFF);
    header[6] = (uint8_t)((len >> 8) & 0xFF);

    // Dynamic verification buffer matching the entire frame contents
    uint8_t *tmp = (uint8_t *)malloc(7 + len);
    if (!tmp)
    {
        ESP_LOGE(TAG, "TX Allocation Failure");
        return;
    }

    memcpy(&tmp[0], header, 7);
    if (len > 0 && payload != NULL)
        memcpy(&tmp[7], payload, len);

    uint8_t crc = calc_crc(tmp, 7 + len);
    free(tmp);

    // Contiguous physical wire send
    uart_write_bytes(UART_PORT, header, 7);
    if (len > 0 && payload != NULL)
    {
        uart_write_bytes(UART_PORT, payload, len);
    }
    uart_write_bytes(UART_PORT, &crc, 1);
}

static void handle_command(uint8_t cmd, uint8_t *payload, uint16_t len)
{
    switch (cmd)
    {
    case CMD_PING:
        ESP_LOGI(TAG, "Received PING, sending PONG");
        uart_send_frame(CMD_PONG, NULL, 0);
        break;

    case CMD_PONG:
        ESP_LOGI(TAG, "Received PONG");
        break;

    case CMD_BOOTED:
        ESP_LOGI(TAG, "C6 reports BOOTED");
        break;

    case CMD_STREAM_GAUGE:
    {
        if (len != sizeof(GaugePacket))
        {
            ESP_LOGW(TAG, "Bad GaugePacket size: %u (expected %u)", len, (unsigned)sizeof(GaugePacket));
            break;
        }

        // SMART FIX: Auto-recover to TELEMETRY mode ONLY if we aren't mid-OTA
        if (current_mode != p4_mode_t::TELEMETRY && !ota_upload_in_progress)
        {
            ESP_LOGI(TAG, "Stray state recovery: Forcing mode back to TELEMETRY");
            current_mode = p4_mode_t::TELEMETRY;
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
    {
        ESP_LOGI(TAG, "CMD_STREAM_FIRMWARE: len=%u", len);
        ota_upload_in_progress = true;
        p4_ota_queue_chunk(payload, len);
        break;
    }

    case CMD_STREAM_FIRMWARE_DONE:
    {
        ESP_LOGI(TAG, "Received OTA DONE from C6");
        ota_upload_in_progress = false;
        p4_ota_finalize_event();
        break;
    }

    case CMD_MODE_P4_OTA:
        if (!ota_upload_in_progress)
        {
            ESP_LOGW(TAG, "Ignoring stray CMD_MODE_P4_OTA (no OTA in progress)");
            break;
        }
        current_mode = p4_mode_t::P4_OTA;
        break;

    case CMD_MODE_TELEMETRY:
        current_mode = p4_mode_t::TELEMETRY;
        break;

    case CMD_C6_UPLOAD_BEGIN:
    {
        ota_upload_in_progress = true;
        uint32_t total_len = 0;
        if (len == sizeof(total_len))
        {
            memcpy(&total_len, payload, sizeof(total_len));
        }

        static ota_event_t begin_evt;
        memset(&begin_evt, 0, sizeof(ota_event_t));

        begin_evt.type = OTA_EVENT_BEGIN;
        begin_evt.len = total_len;

        if (xQueueSend(ota_queue, &begin_evt, portMAX_DELAY) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to enqueue OTA BEGIN event");
            uart_send_frame(CMD_NACK, nullptr, 0);
        }
        break;
    }

    case CMD_C6_UPLOAD_END:
        ota_upload_in_progress = false;
        break;

    default:
        ESP_LOGW(TAG, "Unknown CMD: 0x%02X", cmd);
        break;
    }
}

static void uart_rx_task(void *arg)
{
    uint32_t max_allocation = MAX_PAYLOAD + 16;
    uint8_t *payload = (uint8_t *)malloc(max_allocation);
    uint8_t *tmp_crc_buf = (uint8_t *)malloc(7 + max_allocation);

    if (!payload || !tmp_crc_buf)
    {
        ESP_LOGE(TAG, "Fatal: Unable to allocate dynamic RX framework heap");
        if (payload)
            free(payload);
        if (tmp_crc_buf)
            free(tmp_crc_buf);
        vTaskDelete(NULL);
        return;
    }

    uint32_t shift_reg = 0;
    ESP_LOGI(TAG, "Synchronous Length-Preambled Binary Receiver Running.");

    while (true)
    {
        // PHASE 1: Sync Window Tracking
        uint8_t b;
        int n = uart_read_bytes(UART_PORT, &b, 1, portMAX_DELAY);
        if (n <= 0)
            continue;

        shift_reg = (shift_reg >> 8) | ((uint32_t)b << 24);

        if (shift_reg == FRAME_MAGIC)
        {
            ESP_LOGI(TAG, "[SYNC MATCH] Found MAGIC preamble. Intercepting header...");
            shift_reg = 0; // Clear to prevent recursive loops on sequential bytes

            // PHASE 2: Parse remaining fixed header metadata (CMD [1B] + LEN [2B])
            uint8_t header_meta[3];
            int meta_read = uart_read_bytes(UART_PORT, header_meta, 3, pdMS_TO_TICKS(100));
            if (meta_read != 3)
            {
                ESP_LOGE(TAG, "[SYNC BREAK] Missing header meta! Expected 3B, Got: %d", meta_read);
                continue;
            }

            uint8_t cmd = header_meta[0];
            uint16_t len = header_meta[1] | ((uint16_t)header_meta[2] << 8);

            ESP_LOGI(TAG, "[STREAM LOCK] CMD: 0x%02X | Expected Payload Size: %u bytes", cmd, len);

            if (len > MAX_PAYLOAD)
            {
                ESP_LOGE(TAG, "[OVERSIZE ABORT] Frame length %u bounds error (MAX %d)", len, MAX_PAYLOAD);
                uart_send_frame(CMD_NACK, nullptr, 0);
                continue;
            }

            // PHASE 3: Read Exact Payload and Checksum Block Directly
            uint32_t total_to_fetch = len + 1; // Content Payload + 1 Checksum Byte
            ESP_LOGI(TAG, "[FETCH START] Extracting %u bytes directly from Ring Buffer...", total_to_fetch);

            int total_fetched = 0;
            bool fetch_success = true;

            while (total_fetched < total_to_fetch)
            {
                uint32_t remaining = total_to_fetch - total_fetched;
                uint32_t read_slice = (remaining > 256) ? 256 : remaining;

                int fetched = uart_read_bytes(UART_PORT, &payload[total_fetched], read_slice, pdMS_TO_TICKS(500));
                if (fetched <= 0)
                {
                    ESP_LOGE(TAG, "[TIMEOUT STALL] Pipeline stalled! Fetched %d/%u bytes. Expected slice: %lu",
                             total_fetched, total_to_fetch, (unsigned long)read_slice);
                    fetch_success = false;
                    break;
                }

                total_fetched += fetched;
                // THIS LOG HELPS YOU TRACK STALL POINT GRAPHICALLY:
                ESP_LOGI(TAG, "[COUNTERS] Pulled slice: +%d bytes (%d / %u total)", fetched, total_fetched, total_to_fetch);
            }

            if (!fetch_success)
            {
                uart_send_frame(CMD_NACK, nullptr, 0);
                continue;
            }

            uint8_t wire_crc = payload[len];

            // PHASE 4: Checksum Evaluation
            tmp_crc_buf[0] = (uint8_t)(FRAME_MAGIC & 0xFF);
            tmp_crc_buf[1] = (uint8_t)((FRAME_MAGIC >> 8) & 0xFF);
            tmp_crc_buf[2] = (uint8_t)((FRAME_MAGIC >> 16) & 0xFF);
            tmp_crc_buf[3] = (uint8_t)((FRAME_MAGIC >> 24) & 0xFF);
            tmp_crc_buf[4] = cmd;
            tmp_crc_buf[5] = header_meta[1];
            tmp_crc_buf[6] = header_meta[2];
            if (len > 0)
            {
                memcpy(&tmp_crc_buf[7], payload, len);
            }

            uint8_t calculated_crc = calc_crc(tmp_crc_buf, 7 + len);

            if (calculated_crc == wire_crc)
            {
                ESP_LOGI(TAG, "[SUCCESS] CRC Match (0x%02X). Passing to command handler.", calculated_crc);
                handle_command(cmd, payload, len);
            }
            else
            {
                ESP_LOGE(TAG, "[CRC INVALID] Mismatch! Calc: 0x%02X vs Wire: 0x%02X", calculated_crc, wire_crc);
                uart_send_frame(CMD_NACK, nullptr, 0);
            }
        }
    }

    free(payload);
    free(tmp_crc_buf);
}

void start_uart_rx_task(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 64,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {}};

    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_RTS_PIN, UART_CTS_PIN));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 8192, 8192, 0, NULL, ESP_INTR_FLAG_IRAM));

    xTaskCreate(uart_rx_task, "p4_uart_rx_task", 8192, NULL, 5, NULL);
}