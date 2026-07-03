#pragma once
#include <cstdint>

constexpr uint8_t FRAME_START = 0xAA;
constexpr uint8_t ESCAPE = 0x7D;
constexpr uint16_t MAX_PAYLOAD = 4096;

void uart_send_frame(uint8_t cmd, const uint8_t *payload, uint16_t len);
uint8_t calc_crc(const uint8_t *data, int len);
void start_uart_rx_task();
