// small_gauge.cpp
// Info gauge module: GPS heading, ambient temp, GNSS time, and indicator flags

#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstddef>
#include <cstring>

static const char *TAG = "small_gauge";

/* Cached state for UI update */
static int16_t  s_ambient_temp  = 0;
static uint8_t  s_hour          = 0;
static uint8_t  s_minute        = 0;
static char     s_compass_str[5] = {0};

/* Evaluated indicator flags */
static bool s_ind_wif      = false;
static bool s_ind_washer   = false;
static bool s_ind_low_fuel = false;
static bool s_ind_low_batt = false;
static bool s_ind_eng_temp = false;

static inline uint32_t now_ms(void) {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

extern "C" void small_gauge_init(void)
{
    ESP_LOGI(TAG, "small gauge info screen initialized");
}

extern "C" void small_gauge_deinit(void)
{
    ESP_LOGI(TAG, "deinitialized");
}

/**
 * @brief Step 1: Poll VCAN receiver, parse PGNs & evaluate indicator thresholds.
 * @note Call this OUTSIDE of the lvgl_lock() window.
 */
extern "C" void small_gauge_update(void)
{
    auto& receiver = vcan::Receiver::instance();
    uint8_t buf[256];
    uint8_t len = 0;
    uint32_t last_seen_ms = 0;
    uint32_t latest_rx_ms = 0;

    // Default status values for evaluation
    int16_t  coolant_temp  = 0;
    int16_t  fuel_level    = 0;
    int16_t  battery_level = 0;
    uint16_t digital_pins  = 0;

    // 1. Coolant Temp (PGN_ENGINE_CORE)
    if (receiver.getLastPayload(PGN_ENGINE_CORE, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_engine_core_t, coolant_temp) + sizeof(pgn_engine_core_t::coolant_temp);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_engine_core_t *>(buf);
            coolant_temp = msg->coolant_temp;
            if (last_seen_ms > latest_rx_ms) latest_rx_ms = last_seen_ms;
        }
    }

    // 2. Fuel Level & Digital Pins (PGN_EXHAUST_DIG)
    if (receiver.getLastPayload(PGN_EXHAUST_DIG, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_exhaust_dig_t, digital_pins) + sizeof(pgn_exhaust_dig_t::digital_pins);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t *>(buf);
            fuel_level   = msg->fuel_level;
            digital_pins = msg->digital_pins;
            if (last_seen_ms > latest_rx_ms) latest_rx_ms = last_seen_ms;
        }
    }

    // 3. Battery Voltage (PGN_PRESSURES)
    if (receiver.getLastPayload(PGN_PRESSURES, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_pressures_t, battery_level) + sizeof(pgn_pressures_t::battery_level);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_pressures_t *>(buf);
            battery_level = msg->battery_level;
            if (last_seen_ms > latest_rx_ms) latest_rx_ms = last_seen_ms;
        }
    }

    // 4. Ambient Temp (PGN_TEMPS)
    if (receiver.getLastPayload(PGN_TEMPS, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_temps_t, ambient_temp) + sizeof(pgn_temps_t::ambient_temp);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_temps_t *>(buf);
            s_ambient_temp = msg->ambient_temp;
            if (last_seen_ms > latest_rx_ms) latest_rx_ms = last_seen_ms;
        }
    }

    // 5. GPS Heading / Compass String (PGN_GPS_STATUS)
    if (receiver.getLastPayload(PGN_GPS_STATUS, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_gps_status_t, compass4) + sizeof(pgn_gps_status_t::compass4);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_gps_status_t *>(buf);
            memcpy(s_compass_str, msg->compass4, 4);
            s_compass_str[4] = '\0';
            if (last_seen_ms > latest_rx_ms) latest_rx_ms = last_seen_ms;
        }
    }

    // 6. GNSS Time (PGN_GNSS_TIME)
    if (receiver.getLastPayload(PGN_GNSS_TIME, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_gnss_time_t, minute) + sizeof(pgn_gnss_time_t::minute);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_gnss_time_t *>(buf);
            s_hour   = msg->hour;
            s_minute = msg->minute;
            if (last_seen_ms > latest_rx_ms) latest_rx_ms = last_seen_ms;
        }
    }

    // --- Indicator Evaluation ---

    // 1. Digital pins check using protocol channel bitmasks (VCAN_BIT macro)
    s_ind_wif    = (digital_pins & VCAN_BIT(DIG_WATER_FUEL)) != 0;
    s_ind_washer = (digital_pins & VCAN_BIT(DIG_LOW_WASHER)) != 0;

    // 2. Analog threshold evaluations (scaled by INT_SCALING = 100)
    s_ind_low_fuel = (fuel_level < (10 * INT_SCALING));     // Fuel < 10%
    s_ind_low_batt = (battery_level < (11 * INT_SCALING));  // Battery < 11.0V
    s_ind_eng_temp = (coolant_temp > (235 * INT_SCALING));  // Over-temp safeguard

    // Throttled logging (once per second)
    static uint32_t last_log_ms = 0;
    uint32_t now = now_ms();
    if (now - last_log_ms >= 1000) {
        ESP_LOGI(TAG, "UPDATE Flags | WIF:%d Washer:%d LowFuel:%d LowBatt:%d HighTemp:%d",
                 s_ind_wif, s_ind_washer, s_ind_low_fuel, s_ind_low_batt, s_ind_eng_temp);
        last_log_ms = now;
    }

    if (latest_rx_ms > 0 && (now - latest_rx_ms) > 2000) {
        ESP_LOGW(TAG, "CAN data stale! Last update %lu ms ago", (unsigned long)(now - latest_rx_ms));
    }
}

/**
 * @brief Step 2: Apply updated cached values directly to LVGL widgets.
 * @note Call this INSIDE the lvgl_lock() window.
 */
extern "C" void small_gauge_draw(void)
{
    update_ambient_temp_display(s_ambient_temp);
    update_heading_display(s_compass_str);
    update_time_display(s_hour, s_minute);
    update_indicators_display(s_ind_wif, s_ind_washer, s_ind_low_fuel, s_ind_low_batt, s_ind_eng_temp);
}