// small_gauge.cpp
// Info gauge module: GPS heading, ambient temp, GNSS time, and indicator flags

#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "small_gauge";

typedef struct
{
    int16_t  fuelLevel;      // raw fuel (scaled by INT_SCALING)
    int16_t  batteryLevel;   // raw battery (scaled by INT_SCALING)
    int16_t  coolantTemp;    // raw coolant temp (scaled by INT_SCALING)
    int16_t  ambientTemp;    // raw ambient temp (scaled by INT_SCALING)

    uint16_t digitalPins;    // raw digital bitmap

    char     compass8[8];    // heading string
    uint8_t  hour;
    uint8_t  minute;

    uint32_t daq_uptime_ms;
    uint32_t last_update_ms;
} sg_state_t;

static sg_state_t s_state;
static SemaphoreHandle_t s_mutex = NULL;

static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000ULL); }

/* --- Callbacks using direct Struct Casting --- */

static void cb_engine_core(uint16_t pgn, const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(pgn_engine_core_t)) {
        ESP_LOGW(TAG, "PGN 0x%02X payload short: %d (expected %u)", pgn, len, (unsigned)sizeof(pgn_engine_core_t));
        return;
    }
    const auto *msg = reinterpret_cast<const pgn_engine_core_t *>(payload);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.coolantTemp    = msg->coolant_temp;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_heartbeat(uint16_t pgn, const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(pgn_heartbeat_t)) {
        ESP_LOGW(TAG, "PGN 0x%02X payload short: %d (expected %u)", pgn, len, (unsigned)sizeof(pgn_heartbeat_t));
        return;
    }
    const auto *msg = reinterpret_cast<const pgn_heartbeat_t *>(payload);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.daq_uptime_ms  = msg->uptime_ms;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_exhaust_dig(uint16_t pgn, const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(pgn_exhaust_dig_t)) {
        ESP_LOGW(TAG, "PGN 0x%02X payload short: %d (expected %u)", pgn, len, (unsigned)sizeof(pgn_exhaust_dig_t));
        return;
    }
    const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t *>(payload);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.fuelLevel      = msg->fuel_level;
        s_state.digitalPins    = msg->digital_pins;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_pressures(uint16_t pgn, const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(pgn_pressures_t)) {
        ESP_LOGW(TAG, "PGN 0x%02X payload short: %d (expected %u)", pgn, len, (unsigned)sizeof(pgn_pressures_t));
        return;
    }
    const auto *msg = reinterpret_cast<const pgn_pressures_t *>(payload);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.batteryLevel   = msg->battery_level;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_temps(uint16_t pgn, const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(pgn_temps_t)) {
        ESP_LOGW(TAG, "PGN 0x%02X payload short: %d (expected %u)", pgn, len, (unsigned)sizeof(pgn_temps_t));
        return;
    }
    const auto *msg = reinterpret_cast<const pgn_temps_t *>(payload);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.ambientTemp    = msg->ambient_temp;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_gps_status(uint16_t pgn, const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(pgn_gps_status_t)) {
        ESP_LOGW(TAG, "PGN 0x%02X payload short: %d (expected %u)", pgn, len, (unsigned)sizeof(pgn_gps_status_t));
        return;
    }
    const auto *msg = reinterpret_cast<const pgn_gps_status_t *>(payload);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        memcpy(s_state.compass8, msg->compass4, 4);
        s_state.compass8[4]     = '\0';
        s_state.last_update_ms  = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_gnss_time(uint16_t pgn, const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(pgn_gnss_time_t)) {
        ESP_LOGW(TAG, "PGN 0x%02X payload short: %d (expected %u)", pgn, len, (unsigned)sizeof(pgn_gnss_time_t));
        return;
    }
    const auto *msg = reinterpret_cast<const pgn_gnss_time_t *>(payload);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.hour           = msg->hour;
        s_state.minute         = msg->minute;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

/* --- Init / Deinit --- */

void small_gauge_init(void)
{
    if (!s_mutex)
        s_mutex = xSemaphoreCreateMutex();

    memset(&s_state, 0, sizeof(s_state));

    auto& receiver = vcan::Receiver::instance();

    receiver.registerCallback(PGN_ENGINE_CORE, cb_engine_core);
    receiver.registerCallback(PGN_HEARTBEAT,   cb_heartbeat);
    receiver.registerCallback(PGN_EXHAUST_DIG, cb_exhaust_dig);
    receiver.registerCallback(PGN_PRESSURES,   cb_pressures);
    receiver.registerCallback(PGN_TEMPS,       cb_temps);
    receiver.registerCallback(PGN_GPS_STATUS,  cb_gps_status);
    receiver.registerCallback(PGN_GNSS_TIME,   cb_gnss_time);

    ESP_LOGI(TAG, "small gauge info screen initialized");
}

void small_gauge_deinit(void)
{
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
}

/* --- Draw function --- */

void small_gauge_draw(void)
{
    sg_state_t local;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE)
        return;

    local = s_state;
    xSemaphoreGive(s_mutex);

    // 1. Digital pins check using protocol channel bitmasks (VCAN_BIT macro)
    bool ind_wif    = (local.digitalPins & VCAN_BIT(DIG_WATER_FUEL)) != 0;
    bool ind_washer = (local.digitalPins & VCAN_BIT(DIG_LOW_WASHER)) != 0;

    // 2. Analog threshold evaluations (scaled by INT_SCALING = 100)
    bool ind_low_fuel = (local.fuelLevel < (10 * INT_SCALING));     // Fuel < 10%
    bool ind_low_batt = (local.batteryLevel < (11 * INT_SCALING));  // Battery < 10.0V
    bool ind_eng_temp = (local.coolantTemp > (235 * INT_SCALING));  // Over-temp safeguard

    // Throttled logging (once every second)
    static uint32_t last_log_ms = 0;
    uint32_t now = now_ms();
    if (now - last_log_ms >= 1000) {
        ESP_LOGI(TAG, "DRAW Flags | WIF:%d Washer:%d LowFuel:%d LowBatt:%d HighTemp:%d",
                 ind_wif, ind_washer, ind_low_fuel, ind_low_batt, ind_eng_temp);
        last_log_ms = now;
    }

    // 3. Direct calls to update UI components
    update_ambient_temp_display(local.ambientTemp);
    update_heading_display(local.compass8);
    update_time_display(local.hour, local.minute);
    update_indicators_display(ind_wif, ind_washer, ind_low_fuel, ind_low_batt, ind_eng_temp);

    if ((now - local.last_update_ms) > 2000) {
        ESP_LOGW(TAG, "CAN data stale! Last update %lu ms ago", (unsigned long)(now - local.last_update_ms));
    }
}