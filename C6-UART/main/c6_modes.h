#pragma once
#include <stdint.h>

typedef enum
{
    MODE_TELEMETRY = 0, // Normal ESP-NOW telemetry
    MODE_C6_OTA,        // C6 OTA via HTTP server
    MODE_P4_OTA,        // P4 OTA via UART stream
    MODE_FACTORY_RESET, // Clear provisioning + reboot
    MODE_REBOOT,        // Simple reboot
    MODE_PROVISIONING   // NEW: Wi-Fi provisioning mode (SoftAP)
} c6_mode_t;

extern volatile c6_mode_t current_mode;

// ---------------------------------------------------------
// Command IDs
// ---------------------------------------------------------

// Basic control
#define CMD_PING 0x01
#define CMD_PONG 0x02
#define CMD_DUMMY_DATA 0x05
#define CMD_BOOTED 0x06
#define CMD_ACK 0x07

// Mode control commands (P4 → C6)
#define CMD_MODE_C6_OTA 0x10
#define CMD_MODE_P4_OTA 0x11
#define CMD_MODE_FACTORY_RESET 0x12
#define CMD_MODE_REBOOT 0x13
#define CMD_MODE_TELEMETRY 0x14
#define CMD_MODE_PROVISIONING 0x15 // NEW: P4 triggers provisioning

// Provisioning status (C6 → P4)
#define CMD_PROV_SUCCESS 0x30
#define CMD_PROV_FAIL 0x31
#define CMD_C6_UPLOAD_BEGIN 0x32
#define CMD_C6_UPLOAD_END 0x33
// ---------------------------------------------------------
// DATA STREAM COMMANDS (C6 → P4)
// ---------------------------------------------------------

// Telemetry stream (GaugePacket)
#define CMD_STREAM_GAUGE 0x20

// Firmware OTA stream (binary chunks)
#define CMD_STREAM_FIRMWARE 0x21
#define CMD_STREAM_FIRMWARE_DONE 0x22

// ---------------------------------------------------------
// Mode handlers
// ---------------------------------------------------------
void do_mode_telemetry(void);
void do_mode_c6_ota(void);
void do_mode_p4_ota(void);
void do_mode_factory_reset(void);
void do_mode_reboot(void);
void do_mode_provisioning(void); // NEW

// ACK helper
void send_ack(const char *msg);
