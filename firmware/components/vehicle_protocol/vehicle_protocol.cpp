#include "vehicle_protocol.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "VEHICLE_PROTOCOL";
static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static bool s_peer_initialized = false;
static SemaphoreHandle_t s_vcan_mutex = NULL;

// If true, send fixed-size frames (sizeof(virtual_can_msg_t)).
// If false, send only id+len+payload (5 + len) to reduce airtime.
static const bool VCAN_USE_VARIABLE_LENGTH = true;

esp_err_t vcan_init_transmitter(void) {
    if (!s_vcan_mutex) {
        s_vcan_mutex = xSemaphoreCreateMutex();
        if (!s_vcan_mutex) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    xSemaphoreTake(s_vcan_mutex, portMAX_DELAY);
    if (s_peer_initialized) {
        xSemaphoreGive(s_vcan_mutex);
        return ESP_OK;
    }

    if (!esp_now_is_peer_exist(BROADCAST_MAC)) {
        esp_now_peer_info_t peer_info = {};
        memcpy(peer_info.peer_addr, BROADCAST_MAC, 6);
        peer_info.channel = 0;
        peer_info.encrypt = false;

        esp_err_t err = esp_now_add_peer(&peer_info);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add broadcast peer: %s", esp_err_to_name(err));
            xSemaphoreGive(s_vcan_mutex);
            return err;
        }
    }

    s_peer_initialized = true;
    xSemaphoreGive(s_vcan_mutex);
    return ESP_OK;
}

static esp_err_t vcan_send_raw(const virtual_can_msg_t *msg) {
    // Ensure transmitter initialized
    if (!s_peer_initialized) {
        esp_err_t e = vcan_init_transmitter();
        if (e != ESP_OK) return e;
    }

    if (VCAN_USE_VARIABLE_LENGTH) {
        // send id(4) + len(1) + payload(len)
        uint8_t txbuf[13]; // max 13
        uint8_t txlen = 5 + (msg->len > 8 ? 8 : msg->len);
        // pack id little-endian for wire (consistent with serializers)
        txbuf[0] = (uint8_t)(msg->id & 0xFF);
        txbuf[1] = (uint8_t)((msg->id >> 8) & 0xFF);
        txbuf[2] = (uint8_t)((msg->id >> 16) & 0xFF);
        txbuf[3] = (uint8_t)((msg->id >> 24) & 0xFF);
        txbuf[4] = msg->len;
        if (msg->len) memcpy(&txbuf[5], msg->data, msg->len);
        return esp_now_send(BROADCAST_MAC, txbuf, txlen);
    } else {
        // fixed-size send
        return esp_now_send(BROADCAST_MAC, (const uint8_t *)msg, sizeof(virtual_can_msg_t));
    }
}

esp_err_t vcan_send_message(uint16_t pgn, uint8_t src_addr, uint8_t priority, const uint8_t *payload, uint8_t len) {
    if (!s_vcan_mutex) {
        // ensure mutex exists
        s_vcan_mutex = xSemaphoreCreateMutex();
        if (!s_vcan_mutex) return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_vcan_mutex, portMAX_DELAY);

    virtual_can_msg_t msg;
    msg.id = vcan_build_id(priority, pgn, src_addr);
    msg.len = (len > 8) ? 8 : len;
    // zero buffer then copy payload (wire-format is little-endian)
    memset(msg.data, 0, sizeof(msg.data));
    if (payload && msg.len > 0) {
        memcpy(msg.data, payload, msg.len);
    }

    esp_err_t err = vcan_send_raw(&msg);

    xSemaphoreGive(s_vcan_mutex);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "vcan_send_message failed pgn=0x%04X err=%s", pgn, esp_err_to_name(err));
    }
    return err;
}

// --- DAQ Convenience Sender Implementations (use deterministic LE writes) ---

esp_err_t vcan_send_engine_core(int16_t rpm, int16_t speed, int16_t gear_position, int16_t coolant_temp) {
    uint8_t buf[8];
    vcan_write_u16_le(&buf[0], (uint16_t)rpm);
    vcan_write_u16_le(&buf[2], (uint16_t)speed);
    vcan_write_u16_le(&buf[4], (uint16_t)gear_position);
    vcan_write_u16_le(&buf[6], (uint16_t)coolant_temp);
    return vcan_send_message(PGN_ENGINE_CORE, ADDR_ECU_DAQ, CAN_PRIORITY_HIGH, buf, 8);
}

esp_err_t vcan_send_pressures(int16_t oil_pressure, int16_t fuel_pressure, int16_t boost_pressure, int16_t battery_level) {
    uint8_t buf[8];
    vcan_write_u16_le(&buf[0], (uint16_t)oil_pressure);
    vcan_write_u16_le(&buf[2], (uint16_t)fuel_pressure);
    vcan_write_u16_le(&buf[4], (uint16_t)boost_pressure);
    vcan_write_u16_le(&buf[6], (uint16_t)battery_level);
    return vcan_send_message(PGN_PRESSURES, ADDR_ECU_DAQ, CAN_PRIORITY_MED, buf, 8);
}

esp_err_t vcan_send_temps(int16_t oil_temp, int16_t trans_temp, int16_t ambient_temp, int16_t ia_temp) {
    uint8_t buf[8];
    vcan_write_u16_le(&buf[0], (uint16_t)oil_temp);
    vcan_write_u16_le(&buf[2], (uint16_t)trans_temp);
    vcan_write_u16_le(&buf[4], (uint16_t)ambient_temp);
    vcan_write_u16_le(&buf[6], (uint16_t)ia_temp);
    return vcan_send_message(PGN_TEMPS, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_exhaust_dig(int16_t eg_temp, int16_t fuel_level, uint16_t digital_pins) {
    uint8_t buf[8];
    memset(buf, 0, 8);
    vcan_write_u16_le(&buf[0], (uint16_t)eg_temp);
    vcan_write_u16_le(&buf[2], (uint16_t)fuel_level);
    vcan_write_u16_le(&buf[4], digital_pins);
    return vcan_send_message(PGN_EXHAUST_DIG, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_imu_dynamics(int16_t accel_x, int16_t accel_y, int16_t accel_z) {
    uint8_t buf[8];
    memset(buf, 0, 8);
    vcan_write_u16_le(&buf[0], (uint16_t)accel_x);
    vcan_write_u16_le(&buf[2], (uint16_t)accel_y);
    vcan_write_u16_le(&buf[4], (uint16_t)accel_z);
    return vcan_send_message(PGN_IMU_DYNAMICS, ADDR_ECU_DAQ, CAN_PRIORITY_HIGH, buf, 8);
}

esp_err_t vcan_send_cruise_odo(uint16_t cruise_active, uint16_t cruise_set_value, uint32_t odometer_tenths) {
    uint8_t buf[8];
    vcan_write_u16_le(&buf[0], cruise_active);
    vcan_write_u16_le(&buf[2], cruise_set_value);
    vcan_write_u32_le(&buf[4], odometer_tenths);
    return vcan_send_message(PGN_CRUISE_ODO, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_gps_position(int32_t lat, int32_t lon) {
    uint8_t buf[8];
    vcan_write_u32_le(&buf[0], (uint32_t)lat);
    vcan_write_u32_le(&buf[4], (uint32_t)lon);
    return vcan_send_message(PGN_GPS_POS, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_gps_motion(uint32_t gps_speed, uint16_t gps_altitude, int16_t heading_deg) {
    uint8_t buf[8];
    vcan_write_u32_le(&buf[0], gps_speed);
    vcan_write_u16_le(&buf[4], gps_altitude);
    vcan_write_u16_le(&buf[6], (uint16_t)heading_deg);
    return vcan_send_message(PGN_GPS_MOTION, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_gps_status(uint8_t gps_fix, uint8_t gps_sat_count, const char compass4[4]) {
    uint8_t buf[8];
    memset(buf, 0, 8);
    buf[0] = gps_fix;
    buf[1] = gps_sat_count;
    if (compass4) {
        // treat compass4 as fixed 4-byte ASCII (no NUL required)
        memcpy(&buf[2], compass4, 4);
    }
    return vcan_send_message(PGN_GPS_STATUS, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_gnss_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
    uint8_t buf[8];
    memset(buf, 0, 8);
    vcan_write_u16_le(&buf[0], year);
    buf[2] = month;
    buf[3] = day;
    buf[4] = hour;
    buf[5] = minute;
    buf[6] = second;
    return vcan_send_message(PGN_GNSS_TIME, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

// Heartbeat includes a sequence byte and protocol version; receivers can detect missing cycles
esp_err_t vcan_send_heartbeat(uint8_t seq, uint32_t uptime_ms) {
    uint8_t buf[8];
    memset(buf, 0, 8);
    buf[0] = seq;
    buf[1] = (uint8_t)VCAN_PROTOCOL_VERSION;
    vcan_write_u32_le(&buf[2], uptime_ms);
    return vcan_send_message(PGN_HEARTBEAT, ADDR_ECU_DAQ, CAN_PRIORITY_HIGH, buf, 6);
}

/*
 * Receiver parsing notes (not a full receiver implementation):
 * - If VCAN_USE_VARIABLE_LENGTH is true, receivers must read first 5 bytes: id(4 LE) + len(1), then read len bytes payload.
 * - If fixed-size frames are used, receivers parse virtual_can_msg_t directly (id is host-endian as built).
 * - Use vcan_get_pgn/vcan_get_priority/vcan_get_src to interpret id.
 * - Use vcan_read_u16_le/vcan_read_u32_le to decode multi-byte fields.
 * - Maintain per-PGN last-seen timestamp and a sequence number from PGN_HEARTBEAT to detect missing frames.
 */

