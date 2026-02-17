#include "espnow_receiver.h"
#include "debug.h"
#include "GaugePacket.h"
#include "lvgl.h"
#include "esp_hosted.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "driver/uart.h"

static const char *TAG = "ESP-NOW-RECV";

// -----------------------------------------------------------------------------
// Backend selection
// -----------------------------------------------------------------------------
#define USE_UART_BRIDGE 1 // 1 = UART bridge, 0 = hosted ESP-NOW

// -----------------------------------------------------------------------------
// Common queue + API
// -----------------------------------------------------------------------------
QueueHandle_t espnow_rx_queue = nullptr;

bool espnow_receiver_get(GaugePacket &out)
{
    if (!espnow_rx_queue)
    {
        return false;
    }
    return xQueueReceive(espnow_rx_queue, &out, 0) == pdTRUE;
}
// -----------------------------------------------------------------------------
// Hosted ESP-NOW backend (current implementation)
// -----------------------------------------------------------------------------
#if !USE_UART_BRIDGE

// Hidden internal function from Wi-Fi driver
extern "C" esp_err_t esp_wifi_internal_reg_rxcb(
    wifi_interface_t ifx,
    void (*fn)(void *buffer, uint16_t len, void *eb));

static void wifi_rx_callback(void *buffer, uint16_t len, void *eb)
{
    ESP_LOGI(TAG, "RX callback fired, len=%u", len);

    if (len != sizeof(GaugePacket))
    {
        ESP_LOGW(TAG, "Unexpected packet size: %u", len);
        return;
    }

    GaugePacket pkt;
    memcpy(&pkt, buffer, sizeof(GaugePacket));

    ESP_LOGI(TAG, "GaugePacket received: firstByte=0x%02X",
             ((uint8_t *)buffer)[0]);

    BaseType_t ok = xQueueSendFromISR(espnow_rx_queue, &pkt, nullptr);
    if (!ok)
    {
        ESP_LOGW(TAG, "RX queue full, dropping packet");
    }
}

esp_err_t espnow_receiver_init(uint8_t channel)
{
    ESP_LOGI(TAG, "Initializing hosted ESP-NOW receiver");

    espnow_rx_queue = xQueueCreate(10, sizeof(GaugePacket));
    if (!espnow_rx_queue)
    {
        return ESP_FAIL;
    }

    // Initialize hosted Wi-Fi + ESP-NOW firmware
    ESP_ERROR_CHECK(esp_hosted_init());

    // Register the internal RX callback
    ESP_ERROR_CHECK(esp_wifi_internal_reg_rxcb(WIFI_IF_STA, wifi_rx_callback));

    // Wait for hosted Wi-Fi to come up
    uint8_t primary = 0;
    wifi_second_chan_t second;

    for (int i = 0; i < 50; i++)
    { // wait up to ~500ms
        esp_wifi_get_channel(&primary, &second);
        if (primary != 0)
            break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI("CHANNEL", "C6 is on channel %d", primary);

    if (primary == 0)
    {
        ESP_LOGE(TAG, "C6 Wi-Fi never came up!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Hosted ESP-NOW receiver ready");
    return ESP_OK;
}

#endif // !USE_UART_BRIDGE

// -----------------------------------------------------------------------------
// UART bridge backend (S3 → P4 over UART)
// -----------------------------------------------------------------------------
#if USE_UART_BRIDGE

// Adjust these to match your wiring
static constexpr uart_port_t BRIDGE_UART_NUM = UART_NUM_2;
static constexpr int BRIDGE_UART_RX_PIN = GPIO_NUM_30; // example
static constexpr int BRIDGE_UART_TX_PIN = GPIO_NUM_31; // not used, but set
static constexpr int BRIDGE_UART_BAUD = 921600;

static TaskHandle_t uart_rx_task_handle = nullptr;

static void uart_rx_task(void *param)
{
    static const uint8_t START_BYTE = 0xAA;
    static const uint8_t END_BYTE = 0x55;

    uint8_t byte;
    uint8_t buffer[sizeof(GaugePacket)];
    size_t index = 0;

    enum
    {
        WAIT_START,
        READ_DATA,
        WAIT_END
    } state = WAIT_START;

    ESP_LOGI(TAG, "UART RX task started");

    for (;;)
    {
        int len = uart_read_bytes(BRIDGE_UART_NUM, &byte, 1, pdMS_TO_TICKS(50));
        // ESP_LOGD(TAG, "UART byte: 0x%02X (state=%d)", byte, state);
        if (len <= 0)
        {
            continue;
        }

        switch (state)
        {
        case WAIT_START:
            if (byte == START_BYTE)
            {
                // ESP_LOGI(TAG, "START_BYTE detected");
                index = 0;
                state = READ_DATA;
            }
            break;

        case READ_DATA:
            buffer[index++] = byte;
            if (index == sizeof(GaugePacket))
            {
                // ESP_LOGI(TAG, "GaugePacket payload received (%u bytes)", index);
                state = WAIT_END;
            }
            break;

        case WAIT_END:
            if (byte == END_BYTE)
            {
                // ESP_LOGI(TAG, "END_BYTE detected — full packet ready");

                GaugePacket pkt;
                memcpy(&pkt, buffer, sizeof(GaugePacket));

                ESP_LOGI(TAG, "uart_rx_task queue: %p", espnow_rx_queue);

                BaseType_t ok = xQueueSend(espnow_rx_queue, &pkt, pdMS_TO_TICKS(5));
                if (!ok)
                {
                    ESP_LOGW(TAG, "RX queue full, dropping UART packet");
                }
            }
            state = WAIT_START;
            break;
        }
    }
    taskYIELD();
}

esp_err_t espnow_receiver_init(uint8_t channel)
{
    ESP_LOGI(TAG, "Initializing UART bridge receiver");

    espnow_rx_queue = xQueueCreate(32, sizeof(GaugePacket));
    if (!espnow_rx_queue)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "queue created: %p", espnow_rx_queue);

    // Configure UART
    uart_config_t cfg = {};
    cfg.baud_rate = BRIDGE_UART_BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(BRIDGE_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BRIDGE_UART_NUM,
                                 BRIDGE_UART_TX_PIN,
                                 BRIDGE_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BRIDGE_UART_NUM,
                                        2048, 0, 0, nullptr, 0));

    // Reader task on core 0
    xTaskCreatePinnedToCore(
        uart_rx_task,
        "uart_rx_task",
        4096,
        nullptr,
        5,
        &uart_rx_task_handle,
        0 // core 0
    );

    ESP_LOGI(TAG, "UART bridge receiver ready");
    return ESP_OK;
}

#endif // USE_UART_BRIDGE

void espnow_receiver_pump()
{
    static uint32_t last_update_ms = 0;
    const uint32_t now = lv_tick_get(); // LVGL-safe millisecond timer

    // Only allow updates every 250 ms
    if (now - last_update_ms < 16)
        return;

    GaugePacket pkt;
    if (espnow_receiver_get(pkt))
    {
        last_update_ms = now;
        gauge_state_set(pkt);
        ESP_LOGI("PUMP", "PUMP speed=%d rpm=%d", pkt.speed, pkt.rpm);
    }
}