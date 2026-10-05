#include "small_gauge.h"
#include "esp_log.h"
#include "vcan_receiver.h"
#include "vcan_protocol.h"
#include "updateUI.h"

#include <cstddef>

static const char *TAG = "small_gauge";

/* Local UI smoothing states */
static float s_iat_lerp  = 0.0f;
static float s_egt_lerp  = 0.0f;
static float s_batt_lerp = 0.0f;
static constexpr float LERP_ALPHA = 0.15f;

static inline float lerp_f(float a, float b, float t) {
    return a + (b - a) * t;
}

extern "C" void small_gauge_init(void)
{
    ESP_LOGI(TAG, "small_gauge C++ initialized");
}

extern "C" void small_gauge_deinit(void)
{
    ESP_LOGI(TAG, "deinitialized");
}

extern "C" void small_gauge_draw(void)
{
    auto& receiver = vcan::Receiver::instance();
    uint8_t buf[256];
    uint8_t len = 0;
    uint32_t last_seen_ms = 0;

    // 1. IAT (PGN_TEMPS -> ia_temp)
    if (receiver.getLastPayload(PGN_TEMPS, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_temps_t, ia_temp) + sizeof(pgn_temps_t::ia_temp);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_temps_t *>(buf);
            s_iat_lerp = lerp_f(s_iat_lerp, static_cast<float>(msg->ia_temp), LERP_ALPHA);
            update_iat_meter(static_cast<int32_t>(s_iat_lerp));
        }
    }

    // 2. EGT (PGN_EXHAUST_DIG -> eg_temp)
    if (receiver.getLastPayload(PGN_EXHAUST_DIG, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_exhaust_dig_t, eg_temp) + sizeof(pgn_exhaust_dig_t::eg_temp);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t *>(buf);
            s_egt_lerp = lerp_f(s_egt_lerp, static_cast<float>(msg->eg_temp), LERP_ALPHA);
            update_egt_meter(static_cast<int32_t>(s_egt_lerp));
        }
    }

    // 3. Battery Voltage (PGN_PRESSURES -> battery_level)
    if (receiver.getLastPayload(PGN_PRESSURES, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_pressures_t, battery_level) + sizeof(pgn_pressures_t::battery_level);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_pressures_t *>(buf);
            s_batt_lerp = lerp_f(s_batt_lerp, static_cast<float>(msg->battery_level), LERP_ALPHA);
            update_battery_arc(static_cast<int32_t>(s_batt_lerp));
        }
    }
}