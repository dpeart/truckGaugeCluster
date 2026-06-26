#include "c6_modes.h"
#include "c6_uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_ota.h"
#include "ota_handler.h"
#include "wifi_provisioning/manager.h"
#include <string.h>

#define TAG "C6_MODES"

volatile c6_mode_t current_mode = MODE_TELEMETRY;

void send_ack(const char *msg)
{
    ESP_LOGI(TAG, "%s", msg);
    uart_send_frame(CMD_ACK, (uint8_t *)msg, strlen(msg));
}

/********************************************************************
 * TELEMETRY MODE
 ********************************************************************/
void do_mode_telemetry(void)
{
    ESP_LOGI(TAG, "Switching to TELEMETRY mode");
    send_ack("Telemetry mode active");

    // ESP-NOW telemetry is handled elsewhere (espnow init already done)
}

/********************************************************************
 * C6 OTA MODE (C6 updates itself)
 ********************************************************************/
void do_mode_c6_ota(void)
{
    ESP_LOGI(TAG, "C6 OTA mode active");
    send_ack("Entering C6 OTA mode");

    // Tell wifi_ota.c that OTA target is the C6 itself
    wifi_ota_set_target(OTA_TARGET_C6);

    // This starts Wi-Fi, HTTP server, OTA page, etc.
    enter_ota_mode();

    // Block until mode changes (OTA handler will reboot C6)
    while (current_mode == MODE_C6_OTA)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/********************************************************************
 * P4 OTA MODE (C6 streams firmware to P4)
 ********************************************************************/
void do_mode_p4_ota(void)
{
    ESP_LOGI(TAG, "Entering P4 OTA mode");
    send_ack("Entering P4 OTA mode");

    // Tell wifi_ota.c that OTA target is the P4 (UART streaming)
    wifi_ota_set_target(OTA_TARGET_P4);

    // Enter OTA mode (Wi-Fi STA, connect, download, stream to P4)
    enter_ota_mode();

    // Stay here until OTA completes or mode changes
    while (current_mode == MODE_P4_OTA)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Exiting P4 OTA mode");
}

/********************************************************************
 * FACTORY RESET MODE
 ********************************************************************/
void do_mode_factory_reset(void)
{
    send_ack("Factory reset requested");

    wifi_prov_mgr_reset_provisioning();
    exit_ota_mode();   // ensure Wi-Fi is back to ESP-NOW-only

    ESP_LOGI(TAG, "Factory reset complete, rebooting...");
    esp_restart();
}

/********************************************************************
 * REBOOT MODE
 ********************************************************************/
void do_mode_reboot(void)
{
    send_ack("Rebooting C6");
    ESP_LOGI(TAG, "Rebooting now");

    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

/********************************************************************
 * PROVISIONING MODE
 ********************************************************************/
void do_mode_provisioning(void)
{
    ESP_LOGI(TAG, "Entering PROVISIONING mode");
    send_ack("Entering provisioning mode");

    enter_provisioning_mode();

    // Stay in provisioning until mode changes
    while (current_mode == MODE_PROVISIONING)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Exiting PROVISIONING mode");

    exit_provisioning_mode();
}
