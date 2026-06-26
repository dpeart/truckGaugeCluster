#pragma once
#include <cstdint>

void uart_send_frame(uint8_t cmd, const uint8_t *payload, uint16_t len);
uint8_t calc_crc(const uint8_t *data, int len);
void start_uart_rx_task();
