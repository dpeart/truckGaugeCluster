#ifndef C6_UART_H
#define C6_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define MAX_PAYLOAD 4096

// Unified Protocol Command Opcodes
typedef enum {
    // Basic control
    CMD_PING                 = 0x01,
    CMD_PONG                 = 0x02,
    CMD_STREAM_VCAN_FRAME    = 0x03, // Low-latency raw VCAN frame streaming
    CMD_DUMMY_DATA           = 0x05,
    CMD_BOOTED               = 0x06,
    CMD_ACK                  = 0x07,
    CMD_NACK                 = 0x08,

    // Mode control commands (P4 -> C6)
    CMD_MODE_C6_OTA          = 0x10,
    CMD_MODE_P4_OTA          = 0x11,
    CMD_MODE_FACTORY_RESET   = 0x12,
    CMD_MODE_REBOOT          = 0x13,
    CMD_MODE_TELEMETRY       = 0x14,
    CMD_MODE_PROVISIONING    = 0x15,

    // Data stream commands (C6 -> P4)
    CMD_STREAM_GAUGE         = 0x20, // Legacy
    CMD_STREAM_FIRMWARE      = 0x21,
    CMD_STREAM_FIRMWARE_DONE = 0x22,

    // Provisioning status (C6 -> P4)
    CMD_PROV_SUCCESS         = 0x30,
    CMD_PROV_FAIL            = 0x31,
    CMD_C6_UPLOAD_BEGIN      = 0x32,
    CMD_C6_UPLOAD_END        = 0x33
} c6_uart_cmd_t;

extern volatile c6_uart_cmd_t last_received_cmd;

// API Prototypes
void start_uart_rx_task(void);
void uart_send_frame(c6_uart_cmd_t cmd, const uint8_t *payload, uint16_t len);
void uart_send_vcan_frame(const uint8_t *data, uint16_t len);
void uart_send_firmware_chunk(const uint8_t *data, uint16_t len);
bool wait_for_ack(uint32_t timeout_ms);

#endif // C6_UART_H