#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Virtual CAN Frame Structure (Mirrors an 8-byte CAN frame + 29-bit ID)
typedef struct __attribute__((packed)) {
    uint32_t id;       // 29-bit Extended ID: Priority(3) + PGN(16) + Src(8) (simplified)
    uint8_t  len;      // Payload length (0 to 8 bytes)
    uint8_t  data[8];  // Raw payload bytes (wire-format: little-endian)
} virtual_can_msg_t;

// Core Transmitter API
esp_err_t vcan_init_transmitter(void);
esp_err_t vcan_send_message(uint16_t pgn, uint8_t src_addr, uint8_t priority, const uint8_t *payload, uint8_t len);

// Convenience DAQ Sender Wrappers (pack fields into deterministic LE wire format)
esp_err_t vcan_send_engine_core(int16_t rpm, int16_t speed, int16_t gear_position, int16_t coolant_temp);
esp_err_t vcan_send_pressures(int16_t oil_pressure, int16_t fuel_pressure, int16_t boost_pressure, int16_t battery_level);
esp_err_t vcan_send_temps(int16_t oil_temp, int16_t trans_temp, int16_t ambient_temp, int16_t ia_temp);
esp_err_t vcan_send_exhaust_dig(int32_t eg_temp, int16_t fuel_level, uint16_t digital_pins);
esp_err_t vcan_send_imu_dynamics(int16_t accel_x, int16_t accel_y, int16_t accel_z);
esp_err_t vcan_send_cruise_odo(uint16_t cruise_active, uint16_t cruise_set_value, uint32_t odometer_tenths);
esp_err_t vcan_send_gps_position(int32_t lat, int32_t lon);
esp_err_t vcan_send_gps_motion(uint32_t gps_speed, uint16_t gps_altitude, int16_t heading_deg);
esp_err_t vcan_send_gps_status(uint8_t gps_fix, uint8_t gps_sat_count, const char compass4[4]);
esp_err_t vcan_send_gnss_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
esp_err_t vcan_send_digital_inputs(uint16_t digital_pins);

// Heartbeat / sequence
esp_err_t vcan_send_heartbeat(uint8_t seq, uint32_t uptime_ms);

// Receiver helpers (implement in receiver module)
void vcan_parse_msg(const virtual_can_msg_t *msg); // prototype for receiver-side parser

#ifdef __cplusplus
}
#endif
