#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Source Addresses
#define ADDR_ECU_DAQ      0xFA
#define ADDR_DISPLAY_NODE 0xFB

// Protocol version
#define VCAN_PROTOCOL_VERSION 1

// Standard OpenECU-style Priorities
#define CAN_PRIORITY_HIGH   3
#define CAN_PRIORITY_MED    4
#define CAN_PRIORITY_LOW    6

// Parameter Group Numbers (PGNs) mapped to your GaugePacket groups
#define PGN_ENGINE_CORE    0xFF01
#define PGN_PRESSURES      0xFF02
#define PGN_TEMPS          0xFF03
#define PGN_EXHAUST_DIG    0xFF04
#define PGN_IMU_DYNAMICS   0xFF05
#define PGN_CRUISE_ODO     0xFF06
#define PGN_GPS_POS        0xFF10
#define PGN_GPS_MOTION     0xFF11
#define PGN_GPS_STATUS     0xFF12
#define PGN_GNSS_TIME      0xFF13
#define PGN_HEARTBEAT      0xFF20  // sequence, protocol version, optional uptime

// Virtual CAN Frame Structure (Mirrors an 8-byte CAN frame + 29-bit ID)
typedef struct __attribute__((packed)) {
    uint32_t id;       // 29-bit Extended ID: Priority(3) + PGN(16) + Src(8) (simplified)
    uint8_t  len;      // Payload length (0 to 8 bytes)
    uint8_t  data[8];  // Raw payload bytes (wire-format: little-endian)
} virtual_can_msg_t;

// Inline Identifier Helpers (simplified OpenECU mapping)
static inline uint32_t vcan_build_id(uint8_t priority, uint16_t pgn, uint8_t src_addr) {
    return ((uint32_t)(priority & 0x07) << 26) |
           ((uint32_t)(pgn & 0xFFFF) << 8) |
           (uint32_t)(src_addr & 0xFF);
}
static inline uint8_t vcan_get_priority(uint32_t id) {
    return (uint8_t)((id >> 26) & 0x07);
}
static inline uint16_t vcan_get_pgn(uint32_t id) {
    return (uint16_t)((id >> 8) & 0xFFFF);
}
static inline uint8_t vcan_get_src(uint32_t id) {
    return (uint8_t)(id & 0xFF);
}

// Deterministic little-endian serializers (use these for wire format)
static inline void vcan_write_u16_le(uint8_t *buf, uint16_t v) {
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
}
static inline void vcan_write_u32_le(uint8_t *buf, uint32_t v) {
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    buf[2] = (uint8_t)((v >> 16) & 0xFF);
    buf[3] = (uint8_t)((v >> 24) & 0xFF);
}
static inline uint16_t vcan_read_u16_le(const uint8_t *buf) {
    return (uint16_t)(buf[0] | (buf[1] << 8));
}
static inline uint32_t vcan_read_u32_le(const uint8_t *buf) {
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

// Core Transmitter API
esp_err_t vcan_init_transmitter(void);
esp_err_t vcan_send_message(uint16_t pgn, uint8_t src_addr, uint8_t priority, const uint8_t *payload, uint8_t len);

// Convenience DAQ Sender Wrappers (pack fields into deterministic LE wire format)
esp_err_t vcan_send_engine_core(int16_t rpm, int16_t speed, int16_t gear_position, int16_t coolant_temp);
esp_err_t vcan_send_pressures(int16_t oil_pressure, int16_t fuel_pressure, int16_t boost_pressure, int16_t battery_level);
esp_err_t vcan_send_temps(int16_t oil_temp, int16_t trans_temp, int16_t ambient_temp, int16_t ia_temp);
esp_err_t vcan_send_exhaust_dig(int16_t eg_temp, int16_t fuel_level, uint16_t digital_pins);
esp_err_t vcan_send_imu_dynamics(int16_t accel_x, int16_t accel_y, int16_t accel_z);
esp_err_t vcan_send_cruise_odo(uint16_t cruise_active, uint16_t cruise_set_value, uint32_t odometer_tenths);
esp_err_t vcan_send_gps_position(int32_t lat, int32_t lon);
esp_err_t vcan_send_gps_motion(uint32_t gps_speed, uint16_t gps_altitude, int16_t heading_deg);
esp_err_t vcan_send_gps_status(uint8_t gps_fix, uint8_t gps_sat_count, const char compass4[4]);
esp_err_t vcan_send_gnss_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);

// Heartbeat / sequence
esp_err_t vcan_send_heartbeat(uint8_t seq, uint32_t uptime_ms);

// Receiver helpers (implement in receiver module)
void vcan_parse_msg(const virtual_can_msg_t *msg); // prototype for receiver-side parser

#ifdef __cplusplus
}
#endif
