#pragma once
#include <stdint.h>
#include "GaugePacket.h"

#define FRAME_START 0xAA
#define ESCAPE 0x7D

void uart_send_frame(uint8_t cmd, const uint8_t *payload, uint16_t len);
uint8_t calc_crc(const uint8_t *data, int len);
void start_uart_rx_task(void);

// Stream helpers
void uart_send_gauge_packet(const GaugePacket *pkt);
void uart_send_firmware_chunk(const uint8_t *data, uint16_t len);
