#pragma once
#include <cstdint>

// ---------------------------------------------------------
// P4-side mode enum
// ---------------------------------------------------------
enum class p4_mode_t : uint8_t {
    TELEMETRY = 0,

    // C6-side modes (C6 will reboot)
    C6_OTA,
    C6_FACTORY_RESET,
    C6_REBOOT,

    // P4-side OTA (P4 will reboot)
    P4_OTA,
    // Provisioning mode (C6 will reboot)
    PROVISIONING
};

extern volatile p4_mode_t current_mode;

// ---------------------------------------------------------
// Command IDs (must match C6 exactly)
// ---------------------------------------------------------

// Basic control
constexpr uint8_t CMD_PING        = 0x01;
constexpr uint8_t CMD_PONG        = 0x02;
constexpr uint8_t CMD_DUMMY_DATA  = 0x05;
constexpr uint8_t CMD_BOOTED      = 0x06;
constexpr uint8_t CMD_ACK         = 0x07;
constexpr uint8_t CMD_NACK        = 0x08;

// Mode control commands (P4 → C6)
constexpr uint8_t CMD_MODE_C6_OTA        = 0x10;
constexpr uint8_t CMD_MODE_P4_OTA        = 0x11;
constexpr uint8_t CMD_MODE_C6_FACTORY_RESET = 0x12;
constexpr uint8_t CMD_MODE_C6_REBOOT        = 0x13;
constexpr uint8_t CMD_MODE_TELEMETRY     = 0x14;
constexpr uint8_t CMD_MODE_PROVISIONING   = 0x15;


// Provisioning status (C6 → P4)
constexpr uint8_t CMD_PROV_SUCCESS = 0x30;
constexpr uint8_t CMD_PROV_FAIL    = 0x31;
constexpr uint8_t CMD_C6_UPLOAD_BEGIN = 0x32;
constexpr uint8_t CMD_C6_UPLOAD_END   = 0x33;

// ---------------------------------------------------------
// DATA STREAM COMMANDS (C6 → P4)
// ---------------------------------------------------------

// Telemetry stream (GaugePacket)
constexpr uint8_t CMD_STREAM_GAUGE    = 0x20;

// Firmware OTA stream (binary chunks)
constexpr uint8_t CMD_STREAM_FIRMWARE = 0x21;
constexpr uint8_t CMD_STREAM_FIRMWARE_DONE = 0x22;


// ---------------------------------------------------------
// Mode send helpers (P4 → C6)
// ---------------------------------------------------------
void send_mode_telemetry();
void send_mode_c6_ota();
void send_mode_p4_ota();
void send_mode_factory_reset();
void send_mode_reboot();
void send_mode_provisioning();

// ---------------------------------------------------------
// ACK handler (C6 → P4)
// ---------------------------------------------------------
void handle_ack(const uint8_t *payload, uint8_t len);
