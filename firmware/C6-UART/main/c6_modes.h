#pragma once
#include <stdint.h>
#include "c6_uart.h"

typedef enum
{
    MODE_TELEMETRY = 0, // Normal ESP-NOW telemetry
    MODE_C6_OTA,        // C6 OTA via HTTP server
    MODE_P4_OTA,        // P4 OTA via UART stream
    MODE_FACTORY_RESET, // Clear provisioning + reboot
    MODE_REBOOT,        // Simple reboot
    MODE_PROVISIONING   // Wi-Fi provisioning mode (SoftAP)
} c6_mode_t;

extern volatile c6_mode_t current_mode;

// Mode handlers
void do_mode_telemetry(void);
void do_mode_c6_ota(void);
void do_mode_p4_ota(void);
void do_mode_factory_reset(void);
void do_mode_reboot(void);
void do_mode_provisioning(void);

// ACK helper
void send_ack(const char *msg);