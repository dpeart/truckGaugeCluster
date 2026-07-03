#include "c6_uart.h"
#include "c6_modes.h"
#include "esp_log.h"
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_ota.h"
#include "ota_handler.h"
#include "wifi_provisioning/manager.h"
#include <string.h>
#include "espnow_receiver.h"
#include "telemetry_task.h"
#include "nvs_flash.h"

#define TAG "C6_MAIN"

static c6_mode_t last_mode = -1;

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_NONE);
    esp_log_level_set("*", ESP_LOG_WARN);
    ESP_LOGI(TAG, "Starting C6 Firmware");

    ESP_ERROR_CHECK(nvs_flash_init());

    // WiFi + OTA state machine
    network_setup();
    init_wifi_state_machine();

    // ESP-NOW telemetry receiver
    espnow_receiver_init();

    // UART RX task
    start_uart_rx_task();

    // Telemetry forwarder task (runs forever)
    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);

    // Announce boot to P4
    uart_send_frame(CMD_BOOTED, NULL, 0);

    // Mode-change loop
    c6_mode_t last_mode = -1;

    while (1)
    {
        if (current_mode != last_mode)
        {
            last_mode = current_mode;

            switch (current_mode)
            {
            case MODE_TELEMETRY:
                do_mode_telemetry();
                break;

            case MODE_C6_OTA:
                do_mode_c6_ota();
                break;

            case MODE_P4_OTA:
                do_mode_p4_ota();
                current_mode = MODE_TELEMETRY;
                break;

            case MODE_FACTORY_RESET:
                do_mode_factory_reset();
                current_mode = MODE_TELEMETRY;
                break;

            case MODE_REBOOT:
                do_mode_reboot();
                break;

            case MODE_PROVISIONING:
                do_mode_provisioning();
                current_mode = MODE_TELEMETRY;
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
