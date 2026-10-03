
#include "GaugePacket.h"
#include "c6_modes.h"
#include "c6_uart.h"
#include "espnow_receiver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

void telemetry_task(void *arg)
{
    GaugePacket pkt;

    while (1)
    {
        if (current_mode == MODE_TELEMETRY)
        {
            memcpy(&pkt, (const void *)&g_latest_gauge, sizeof(GaugePacket));
            uart_send_gauge_packet(&pkt);
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // 20 Hz
    }
}
