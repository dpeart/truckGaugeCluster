// vcan_sender.cpp
#include "vcan_protocol.h"
#include "vcan_sender.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "VCAN-SENDER";
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool s_peer_initialized = false;
static SemaphoreHandle_t s_vcan_mutex = NULL;

// If true, the code previously attempted to send a len byte.
// We now always send id(4 LE) + payload(len) to match vcan_receiver expectations.
static const bool VCAN_USE_VARIABLE_LENGTH = false;

// Ensure esp_now is initialized and broadcast peer exists.
esp_err_t vcan_init_transmitter(void)
{
    if (!s_vcan_mutex)
    {
        s_vcan_mutex = xSemaphoreCreateMutex();
        if (!s_vcan_mutex)
        {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    xSemaphoreTake(s_vcan_mutex, portMAX_DELAY);
    if (s_peer_initialized)
    {
        xSemaphoreGive(s_vcan_mutex);
        return ESP_OK;
    }

    // Ensure esp_now is initialized (safe to call if already initialized)
    esp_err_t err = esp_now_init();
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_NOT_INIT)
    {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        xSemaphoreGive(s_vcan_mutex);
        return err;
    }

    // Add broadcast peer if not present
    if (!esp_now_is_peer_exist(BROADCAST_MAC))
    {
        esp_now_peer_info_t peer_info = {};
        memcpy(peer_info.peer_addr, BROADCAST_MAC, 6);
        peer_info.channel = 0; // 0 = current channel; set explicit channel if you need it
        peer_info.encrypt = false;
#if defined(WIFI_IF_STA)
        peer_info.ifidx = WIFI_IF_STA;
#elif defined(ESP_IF_WIFI_STA)
        peer_info.ifidx = ESP_IF_WIFI_STA;
#endif

        err = esp_now_add_peer(&peer_info);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "esp_now_add_peer returned %s", esp_err_to_name(err));
            // Not fatal in many setups; continue but mark as initialized so we don't loop
        }
    }

    s_peer_initialized = true;
    xSemaphoreGive(s_vcan_mutex);
    ESP_LOGI(TAG, "vcan transmitter initialized");
    return ESP_OK;
}

// Internal: send id(4 LE) + payload(len) OR fixed-size virtual_can_msg_t if you prefer.
// We default to id+payload to minimize airtime and match vcan_receiver.
static esp_err_t vcan_send_raw(const virtual_can_msg_t *msg)
{
    if (!s_peer_initialized)
    {
        esp_err_t e = vcan_init_transmitter();
        if (e != ESP_OK)
            return e;
    }

    // Build id (4 bytes LE) + payload (0..8)
    uint8_t txbuf[12]; // 4 + up to 8
    uint8_t payload_len = (msg->len > 8) ? 8 : msg->len;
    txbuf[0] = (uint8_t)(msg->id & 0xFF);
    txbuf[1] = (uint8_t)((msg->id >> 8) & 0xFF);
    txbuf[2] = (uint8_t)((msg->id >> 16) & 0xFF);
    txbuf[3] = (uint8_t)((msg->id >> 24) & 0xFF);
    if (payload_len)
    {
        memcpy(&txbuf[4], msg->data, payload_len);
    }
    size_t txlen = 4 + payload_len;

    // Log the PGN/length and a hex dump of the bytes before sending
    ESP_LOGI(TAG, "Sending ESP-NOW packet | Length: %d", txlen);
    ESP_LOG_BUFFER_HEX(TAG, txbuf, txlen);

    esp_err_t err = esp_now_send(BROADCAST_MAC, txbuf, txlen);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGD(TAG, "esp_now_send success");
    }
    return err;
}

esp_err_t vcan_send_message(uint16_t pgn, uint8_t src_addr, uint8_t priority, const uint8_t *payload, uint8_t len)
{
    if (!s_vcan_mutex)
    {
        s_vcan_mutex = xSemaphoreCreateMutex();
        if (!s_vcan_mutex)
            return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_vcan_mutex, portMAX_DELAY);

    virtual_can_msg_t msg;
    msg.id = vcan_build_id(priority, pgn, src_addr);
    msg.len = (len > 8) ? 8 : len;
    memset(msg.data, 0, sizeof(msg.data));
    if (payload && msg.len > 0)
    {
        memcpy(msg.data, payload, msg.len);
    }

    esp_err_t err = vcan_send_raw(&msg);

    xSemaphoreGive(s_vcan_mutex);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "vcan_send_message failed pgn=0x%04X err=%s", pgn, esp_err_to_name(err));
    }
    return err;
}

// --- DAQ Convenience Sender Implementations (deterministic LE writes) ---

esp_err_t vcan_send_engine_core(int16_t rpm, int16_t speed, int16_t gear_position, int16_t coolant_temp)
{
    uint8_t buf[8];
    vcan_write_u16_le(&buf[0], (uint16_t)rpm);
    vcan_write_u16_le(&buf[2], (uint16_t)speed);
    vcan_write_u16_le(&buf[4], (uint16_t)gear_position);
    vcan_write_u16_le(&buf[6], (uint16_t)coolant_temp);
    return vcan_send_message(PGN_ENGINE_CORE, ADDR_ECU_DAQ, CAN_PRIORITY_HIGH, buf, 8);
}

esp_err_t vcan_send_pressures(int16_t oil_pressure, int16_t fuel_pressure, int16_t boost_pressure, int16_t battery_level)
{
    uint8_t buf[8];
    vcan_write_u16_le(&buf[0], (uint16_t)oil_pressure);
    vcan_write_u16_le(&buf[2], (uint16_t)fuel_pressure);
    vcan_write_u16_le(&buf[4], (uint16_t)boost_pressure);
    vcan_write_u16_le(&buf[6], (uint16_t)battery_level);
    return vcan_send_message(PGN_PRESSURES, ADDR_ECU_DAQ, CAN_PRIORITY_MED, buf, 8);
}

esp_err_t vcan_send_temps(int16_t oil_temp, int16_t trans_temp, int16_t ambient_temp, int16_t ia_temp)
{
    uint8_t buf[8];
    vcan_write_u16_le(&buf[0], (uint16_t)oil_temp);
    vcan_write_u16_le(&buf[2], (uint16_t)trans_temp);
    vcan_write_u16_le(&buf[4], (uint16_t)ambient_temp);
    vcan_write_u16_le(&buf[6], (uint16_t)ia_temp);
    return vcan_send_message(PGN_TEMPS, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_exhaust_dig(int32_t eg_temp, int16_t fuel_level, uint16_t digital_pins)
{
    uint8_t buf[8];
    memset(buf, 0, 8);
    vcan_write_u32_le(&buf[0], (int32_t)eg_temp);
    vcan_write_u16_le(&buf[4], (uint16_t)fuel_level);
    vcan_write_u16_le(&buf[6], digital_pins);
    return vcan_send_message(PGN_EXHAUST_DIG, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_imu_dynamics(int16_t accel_x, int16_t accel_y, int16_t accel_z)
{
    uint8_t buf[8];
    memset(buf, 0, 8);
    vcan_write_u16_le(&buf[0], (uint16_t)accel_x);
    vcan_write_u16_le(&buf[2], (uint16_t)accel_y);
    vcan_write_u16_le(&buf[4], (uint16_t)accel_z);
    return vcan_send_message(PGN_IMU_DYNAMICS, ADDR_ECU_DAQ, CAN_PRIORITY_HIGH, buf, 8);
}

esp_err_t vcan_send_cruise_odo(uint16_t cruise_active, uint16_t cruise_set_value, uint32_t odometer_tenths)
{
    uint8_t buf[8];
    vcan_write_u16_le(&buf[0], cruise_active);
    vcan_write_u16_le(&buf[2], cruise_set_value);
    vcan_write_u32_le(&buf[4], odometer_tenths);
    return vcan_send_message(PGN_CRUISE_ODO, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_gps_position(int32_t lat, int32_t lon)
{
    uint8_t buf[8];
    vcan_write_u32_le(&buf[0], (uint32_t)lat);
    vcan_write_u32_le(&buf[4], (uint32_t)lon);
    return vcan_send_message(PGN_GPS_POS, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_gps_motion(uint32_t gps_speed, uint16_t gps_altitude, int16_t heading_deg)
{
    uint8_t buf[8];
    vcan_write_u32_le(&buf[0], gps_speed);
    vcan_write_u16_le(&buf[4], gps_altitude);
    vcan_write_u16_le(&buf[6], (uint16_t)heading_deg);
    return vcan_send_message(PGN_GPS_MOTION, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_gps_status(uint8_t gps_fix, uint8_t gps_sat_count, const char compass4[4])
{
    uint8_t buf[8];
    memset(buf, 0, 8);
    buf[0] = gps_fix;
    buf[1] = gps_sat_count;
    if (compass4)
    {
        memcpy(&buf[2], compass4, 4);
    }
    return vcan_send_message(PGN_GPS_STATUS, ADDR_ECU_DAQ, CAN_PRIORITY_LOW, buf, 8);
}

esp_err_t vcan_send_gnss_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
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

esp_err_t vcan_send_heartbeat(uint8_t seq, uint32_t uptime_ms)
{
    uint8_t buf[8];
    memset(buf, 0, 8);
    buf[0] = seq;
    buf[1] = (uint8_t)VCAN_PROTOCOL_VERSION;
    vcan_write_u32_le(&buf[2], uptime_ms);
    // send only 6 bytes (seq + version + uptime32)
    return vcan_send_message(PGN_HEARTBEAT, ADDR_ECU_DAQ, CAN_PRIORITY_HIGH, buf, 6);
}
