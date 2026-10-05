// small_gauge.cpp
#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstddef>

static const char *TAG = "small_gauge";

/* Smoothing state */
static float s_oil_lerp = 0.0f;
static float s_boost_lerp = 0.0f;
static const float LERP_ALPHA = 0.15f;

void small_gauge_init(void)
{
    ESP_LOGI(TAG, "initialized (boost+oil)");
}

void small_gauge_deinit(void)
{
    ESP_LOGI(TAG, "deinitialized");
}

void small_gauge_draw(void)
{
    auto& receiver = vcan::Receiver::instance();
    uint8_t buf[256];
    uint8_t len = 0;
    uint32_t last_seen_ms = 0;

    // Rate-limit log output so terminal isn't overwhelmed
    static uint32_t last_log_ms = 0;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    bool should_log = (now - last_log_ms >= 500);
    if (should_log) last_log_ms = now;

    // 1. Oil Temp (PGN_TEMPS)
    if (receiver.getLastPayload(PGN_TEMPS, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_temps_t, oil_temp) + sizeof(pgn_temps_t::oil_temp);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_temps_t *>(buf);
            float oil_val = static_cast<float>(msg->oil_temp);

            s_oil_lerp += (oil_val - s_oil_lerp) * LERP_ALPHA;

            if (should_log) {
                ESP_LOGI(TAG, "OIL  -> Raw: %d | Scaled: %.2f | Lerp: %.2f | Passing to UI: %d",
                         msg->oil_temp, oil_val, s_oil_lerp, static_cast<int32_t>(s_oil_lerp));
            }

            update_oil_temp_meter(static_cast<int32_t>(s_oil_lerp));
        }
    }

    // 2. Boost Pressure (PGN_PRESSURES)
    if (receiver.getLastPayload(PGN_PRESSURES, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_pressures_t, boost_pressure) + sizeof(pgn_pressures_t::boost_pressure);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_pressures_t *>(buf);
            float boost_val = static_cast<float>(msg->boost_pressure);

            s_boost_lerp += (boost_val - s_boost_lerp) * LERP_ALPHA;

            if (should_log) {
                ESP_LOGI(TAG, "BOOST -> Raw: %d | Scaled: %.2f | Lerp: %.2f | Passing to UI: %d",
                         msg->boost_pressure, boost_val, s_boost_lerp, static_cast<int32_t>(s_boost_lerp));
            }

            update_boost_pressure_meter(static_cast<int32_t>(s_boost_lerp));
        }
    }
}