#include "c6_uart.h"
#include "c6_modes.h"
#include "GaugePacket.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
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
#define UART_RTS_PIN GPIO_NUM_23
#define UART_CTS_PIN GPIO_NUM_22
#define UART_BAUD 460800

#define FRAME_MAGIC 0x54414F50

static SemaphoreHandle_t ack_sem = NULL;
static SemaphoreHandle_t uart_mutex = NULL;

bool ota_upload_in_progress = false;
uint8_t last_received_cmd = 0;
bool wait_for_ack(uint32_t timeout_ms);

uint8_t calc_crc(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++)
        crc ^= data[i];
    return crc;
}

// Synchronous Transmission Protocol (C6 Side)
void uart_send_frame(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    if (!uart_mutex)
        return;

    if (len > MAX_PAYLOAD)
        len = MAX_PAYLOAD;

    if (xSemaphoreTake(uart_mutex, portMAX_DELAY) == pdTRUE)
    {
        uint32_t magic = FRAME_MAGIC;
        static uint8_t header[7];

        header[0] = (uint8_t)(magic & 0xFF);
        header[1] = (uint8_t)((magic >> 8) & 0xFF);
        header[2] = (uint8_t)((magic >> 16) & 0xFF);
        header[3] = (uint8_t)((magic >> 24) & 0xFF);
        header[4] = cmd;
        header[5] = (uint8_t)(len & 0xFF);
        header[6] = (uint8_t)((len >> 8) & 0xFF);

        // Scratchpad validation calculation
        uint8_t *tmp = (uint8_t *)malloc(7 + len);
        if (!tmp)
        {
            ESP_LOGE(TAG, "TX Heap allocation failure");
            xSemaphoreGive(uart_mutex);
            return;
        }

        memcpy(&tmp[0], header, 7);
        if (len > 0 && payload != NULL)
            memcpy(&tmp[7], payload, len);

        uint8_t crc = calc_crc(tmp, 7 + len);
        free(tmp);

        // Hardware Flow Backpressure verification
        size_t free_size = 0;
        while (1)
        {
            if (uart_get_tx_buffer_free_size(UART_PORT, &free_size) == ESP_OK)
            {
                if (free_size >= (7 + len + 1))
                {
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Transmit out contiguous raw components directly
        uart_write_bytes(UART_PORT, header, 7);
        if (len > 0 && payload != NULL)
        {
            uart_write_bytes(UART_PORT, payload, len);
        }
        uart_write_bytes(UART_PORT, &crc, 1);

        xSemaphoreGive(uart_mutex);
    }
}

bool wait_for_ack(uint32_t timeout_ms)
{
    if (ack_sem == NULL)
        return false;

    last_received_cmd = 0;

    if (xSemaphoreTake(ack_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
    {
        if (last_received_cmd == CMD_ACK)
        {
            return true;
        }
        if (last_received_cmd == CMD_NACK)
        {
            return false;
        }
    }

    ESP_LOGE("UART_ACK", "Timeout waiting for ACK after %lu ms", timeout_ms);
    return false;
}

void uart_send_gauge_packet(const GaugePacket *pkt)
{
    uart_send_frame(CMD_STREAM_GAUGE, (const uint8_t *)pkt, sizeof(GaugePacket));
}

void uart_send_firmware_chunk(const uint8_t *data, uint16_t len)
{
    while (len > 0)
    {
        uint16_t chunk_len = (len > MAX_PAYLOAD) ? MAX_PAYLOAD : len;

        ESP_LOGI(TAG, "Sending firmware frame to P4, size: %u bytes", chunk_len);
        uart_send_frame(CMD_STREAM_FIRMWARE, data, chunk_len);

        if (wait_for_ack(3000))
        {
            if (last_received_cmd == CMD_ACK)
            {
                data += chunk_len;
                len -= chunk_len;
            }
            else if (last_received_cmd == CMD_NACK)
            {
                ESP_LOGW(TAG, "P4 rejected chunk (NACK), retrying...");
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        else
        {
            ESP_LOGE(TAG, "UART timeout waiting for P4 4KB response");
            return;
        }
    }
}

static void handle_command(uint8_t cmd, uint8_t *payload, uint16_t len)
{
    switch (cmd)
    {
    case CMD_ACK:
    case CMD_NACK:
        last_received_cmd = cmd;
        if (ack_sem)
        {
            xSemaphoreGive(ack_sem);
        }
        break;

    case CMD_PING:
        ESP_LOGI(TAG, "Received PING from P4 (Reboot/Sync detected). Forcing state to TELEMETRY mode.");

        // 1. Force the C6 mode back to normal operation
        current_mode = MODE_TELEMETRY;
        ota_upload_in_progress = false;

        // 2. Shut down any active OTA network listeners/Wi-Fi configurations on the C6
        exit_ota_mode();

        // 3. Acknowledge back to the P4 that the handshake is successful
        uart_send_frame(CMD_PONG, NULL, 0);
        break;

    case CMD_PONG:
        ESP_LOGI(TAG, "Received PONG");
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
        exit_ota_mode();
        current_mode = MODE_TELEMETRY;
        ota_upload_in_progress = false;
        break;

    case CMD_DUMMY_DATA:
        ESP_LOGI(TAG, "Telemetry: %02X %02X %02X", payload[0], payload[1], payload[2]);
        break;

    default:
        ESP_LOGW(TAG, "Unknown CMD: 0x%02X", cmd);
        break;
    }
}

// Synchronous Length-Preambled Receiver (C6 Side)
static void uart_rx_task(void *arg)
{
    // The C6 usually processes small inbound messages (ACK, NACK, PONGS)
    // but we use a robust dynamic buffer for stability
    uint32_t allocation_sz = 512;
    uint8_t *payload = (uint8_t *)malloc(allocation_sz);
    uint8_t *tmp_crc_buf = (uint8_t *)malloc(7 + allocation_sz);

    if (!payload || !tmp_crc_buf)
    {
        ESP_LOGE(TAG, "RX Buff Allocation Error");
        if (payload)
            free(payload);
        if (tmp_crc_buf)
            free(tmp_crc_buf);
        vTaskDelete(NULL);
        return;
    }

    uint32_t shift_reg = 0;

    while (true)
    {
        uint8_t b;
        int n = uart_read_bytes(UART_PORT, &b, 1, portMAX_DELAY);
        if (n <= 0)
            continue;

        shift_reg = (shift_reg >> 8) | ((uint32_t)b << 24);

        if (shift_reg == FRAME_MAGIC)
        {
            shift_reg = 0;

            uint8_t header_meta[3];
            if (uart_read_bytes(UART_PORT, header_meta, 3, pdMS_TO_TICKS(50)) != 3)
                continue;

            uint8_t cmd = header_meta[0];
            uint16_t len = header_meta[1] | ((uint16_t)header_meta[2] << 8);

            if (len > allocation_sz)
            {
                // Drop if inbound data exceeds our local allocated array sizes
                continue;
            }

            uint32_t fetch_bytes = len + 1;
            if (uart_read_bytes(UART_PORT, payload, fetch_bytes, pdMS_TO_TICKS(200)) != fetch_bytes)
                continue;

            uint8_t wire_crc = payload[len];

            tmp_crc_buf[0] = (uint8_t)(FRAME_MAGIC & 0xFF);
            tmp_crc_buf[1] = (uint8_t)((FRAME_MAGIC >> 8) & 0xFF);
            tmp_crc_buf[2] = (uint8_t)((FRAME_MAGIC >> 16) & 0xFF);
            tmp_crc_buf[3] = (uint8_t)((FRAME_MAGIC >> 24) & 0xFF);
            tmp_crc_buf[4] = cmd;
            tmp_crc_buf[5] = header_meta[1];
            tmp_crc_buf[6] = header_meta[2];
            if (len > 0)
                memcpy(&tmp_crc_buf[7], payload, len);

            if (calc_crc(tmp_crc_buf, 7 + len) == wire_crc)
            {
                handle_command(cmd, payload, len);
            }
        }
    }

    free(payload);
    free(tmp_crc_buf);
}

void start_uart_rx_task(void)
{
    if (uart_mutex == NULL)
    {
        uart_mutex = xSemaphoreCreateMutex();
    }
    if (ack_sem == NULL)
    {
        ack_sem = xSemaphoreCreateBinary();
    }

    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 64,
        .source_clk = UART_SCLK_DEFAULT};

    // Allocate 8192 byte driver ring buffers to accept big packets safely
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 8192, 8192, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_RTS_PIN, UART_CTS_PIN));

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 5, NULL);
}