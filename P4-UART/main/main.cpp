#include "SlaveBootHandler.h"
#include "p4_uart.h"
#include "p4_modes.h"
#include "p4_ota.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "P4_MAIN";

static SlaveBootHandler g_boot;

// static void mode_cycle_task(void *arg)
// {
//     p4_mode_t last_mode = p4_mode_t::TELEMETRY;

//     while (true) {
//         if (current_mode != last_mode) {
//             switch (current_mode) {
//                 case p4_mode_t::TELEMETRY:     send_mode_telemetry(); break;
//                 case p4_mode_t::C6_OTA:        send_mode_c6_ota(); break;
//                 case p4_mode_t::P4_OTA:        send_mode_p4_ota(); break;
//                 case p4_mode_t::C6_FACTORY_RESET: send_mode_factory_reset(); break;
//                 case p4_mode_t::C6_REBOOT:        send_mode_reboot(); break;
//                 case p4_mode_t::PROVISIONING:  send_mode_provisioning(); break;
//             }
//             last_mode = current_mode;
//         }

//         vTaskDelay(pdMS_TO_TICKS(50)); // fast reaction, low CPU
//     }
// }

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting P4 Firmware");

    // Start the boot handler task (this configures EN/IO9 and monitors GPIO51)
    g_boot.begin(5);

    p4_ota_init(); // Initialize OTA queue and task

    // Start UART RX
    start_uart_rx_task();

    // Initial PING to C6
    uart_send_frame(CMD_PING, nullptr, 0);

    // Start in Telemetry mode
    current_mode = p4_mode_t::TELEMETRY;



    // Test mode cycling
    // xTaskCreate(mode_cycle_task, "mode_cycle_task", 4096, nullptr, 5, nullptr);
}
