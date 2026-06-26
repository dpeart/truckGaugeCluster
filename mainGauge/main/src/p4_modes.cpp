#include <cstring>
#include "p4_modes.h"
#include "p4_uart.h"
#include "esp_log.h"

static const char *TAG = "P4_MODES";

volatile p4_mode_t current_mode = p4_mode_t::TELEMETRY;

void send_mode_telemetry()
{
    ESP_LOGI(TAG, "Sending TELEMETRY mode to C6");
    uart_send_frame(CMD_MODE_TELEMETRY, nullptr, 0);
    current_mode = p4_mode_t::TELEMETRY;
}

void send_mode_c6_ota()
{
    ESP_LOGI(TAG, "Sending C6 OTA mode to C6");
    uart_send_frame(CMD_MODE_C6_OTA, nullptr, 0);
    current_mode = p4_mode_t::C6_OTA;
}

void send_mode_p4_ota()
{
    ESP_LOGI(TAG, "Sending P4 OTA mode to C6");
    uart_send_frame(CMD_MODE_P4_OTA, nullptr, 0);
    current_mode = p4_mode_t::P4_OTA;
}

void send_mode_factory_reset()
{
    ESP_LOGI(TAG, "Sending C6 FACTORY RESET mode to C6");
    uart_send_frame(CMD_MODE_C6_FACTORY_RESET, nullptr, 0);
    current_mode = p4_mode_t::C6_FACTORY_RESET;
}

void send_mode_reboot()
{
    ESP_LOGI(TAG, "Sending C6 REBOOT mode to C6");
    uart_send_frame(CMD_MODE_C6_REBOOT, nullptr, 0);
    current_mode = p4_mode_t::C6_REBOOT;
}

void send_mode_provisioning()
{
    ESP_LOGI(TAG, "Sending C6 PROVISIONING mode request");
    uart_send_frame(CMD_MODE_PROVISIONING, nullptr, 0);
    current_mode = p4_mode_t::PROVISIONING;
}

void handle_ack(const uint8_t *payload, uint8_t len)
{
    char msg[64] = {0};

    // Copy payload into a null-terminated string
    if (payload && len > 0)
    {
        uint8_t copy_len = (len < sizeof(msg) - 1) ? len : sizeof(msg) - 1;
        memcpy(msg, payload, copy_len);
        ESP_LOGI(TAG, "ACK from C6: %s", msg);
    }
    else
    {
        ESP_LOGI(TAG, "ACK from C6 (no payload)");
        return;
    }

    // ---------------------------------------------------------
    // MODE TRANSITIONS BASED ON EXACT C6 ACK STRINGS
    // ---------------------------------------------------------

    if (strcmp(msg, "Telemetry mode active") == 0)
    {
        ESP_LOGI(TAG, "Switching to TELEMETRY mode");
        current_mode = p4_mode_t::TELEMETRY;
    }

    else if (strcmp(msg, "Entering C6 OTA mode") == 0)
    {
        ESP_LOGI(TAG, "C6 confirmed OTA mode");
        current_mode = p4_mode_t::C6_OTA;
        // ota_enter_timestamp = xTaskGetTickCount();   // start OTA grace period
    }

    else if (strcmp(msg, "Entering P4 OTA mode") == 0)
    {
        ESP_LOGI(TAG, "C6 acknowledged P4 OTA request");
        current_mode = p4_mode_t::P4_OTA;
    }

    else if (strcmp(msg, "Factory reset requested") == 0)
    {
        ESP_LOGW(TAG, "C6 confirmed FACTORY RESET");
        current_mode = p4_mode_t::C6_FACTORY_RESET;
    }

    else if (strcmp(msg, "Rebooting C6") == 0)
    {
        ESP_LOGW(TAG, "C6 confirmed REBOOT");
        current_mode = p4_mode_t::C6_REBOOT;
    }

    else if (strcmp(msg, "Entering provisioning mode") == 0)
    {
        ESP_LOGI(TAG, "C6 entered PROVISIONING mode");
        current_mode = p4_mode_t::PROVISIONING;
    }

    else
    {
        ESP_LOGW(TAG, "Unknown ACK message: %s", msg);
    }
}
