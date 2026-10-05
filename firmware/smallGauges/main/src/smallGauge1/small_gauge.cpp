#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"

#include "esp_log.h"
#include <cstddef>

static const char *TAG = "small_gauge";

/* Local UI smoothing state */
static float s_coolant_lerp = 0.0f;
static float s_oil_lerp     = 0.0f;
static float s_fuel_lerp    = 0.0f;
static constexpr float LERP_ALPHA = 0.15f;

static inline float lerp_f(float a, float b, float t) {
    return a + (b - a) * t;
}

extern "C" void small_gauge_init(void)
{
    ESP_LOGI(TAG, "initialized small_gauge");
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

    // 1. Coolant Temp (PGN_ENGINE_CORE)
    if (receiver.getLastPayload(PGN_ENGINE_CORE, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_engine_core_t, coolant_temp) + sizeof(pgn_engine_core_t::coolant_temp);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_engine_core_t *>(buf);
            s_coolant_lerp = lerp_f(s_coolant_lerp, static_cast<float>(msg->coolant_temp), LERP_ALPHA);
            update_coolant_meter(static_cast<int32_t>(s_coolant_lerp));
        }
    }

    // 2. Oil Pressure (PGN_PRESSURES)
    if (receiver.getLastPayload(PGN_PRESSURES, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_pressures_t, oil_pressure) + sizeof(pgn_pressures_t::oil_pressure);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_pressures_t *>(buf);
            s_oil_lerp = lerp_f(s_oil_lerp, static_cast<float>(msg->oil_pressure), LERP_ALPHA);
            update_oil_pressure_meter(static_cast<int32_t>(s_oil_lerp));
        }
    }

    // 3. Fuel Level (PGN_EXHAUST_DIG)
    if (receiver.getLastPayload(PGN_EXHAUST_DIG, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_exhaust_dig_t, fuel_level) + sizeof(pgn_exhaust_dig_t::fuel_level);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t *>(buf);
            s_fuel_lerp = lerp_f(s_fuel_lerp, static_cast<float>(msg->fuel_level), LERP_ALPHA);
            update_fuel_arc(static_cast<int32_t>(s_fuel_lerp));
        }
    }
}