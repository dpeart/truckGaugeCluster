#ifndef P4_UART_H
#define P4_UART_H

#include <stdint.h>
#include <stdbool.h>

constexpr uint8_t FRAME_START = 0xAA;
constexpr uint8_t ESCAPE = 0x7D;
constexpr uint16_t MAX_PAYLOAD = 4096;

#ifdef __cplusplus
extern "C" {
#endif

void start_uart_rx_task(void);
void uart_send_frame(uint8_t msg_type, const uint8_t *payload, uint16_t length);

extern bool ota_upload_in_progress;

#ifdef __cplusplus
}
#endif

#endif // P4_UART_H