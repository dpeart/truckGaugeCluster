#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"

#include "esp_log.h"
#include <cstddef>

static const char *TAG = "small_gauge";

/* Local UI LERP smoothing states */
static float s_trans_lerp = 0.0f;
static float s_fuel_lerp  = 0.0f;
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

/**
 * @brief Step 1: Poll VCAN receiver & compute LERP values.
 * @note Call this OUTSIDE of the lvgl_lock() window.
 */
extern "C" void small_gauge_update(void)
{
    auto& receiver = vcan::Receiver::instance();
    uint8_t buf[256];
    uint8_t len = 0;
    uint32_t last_seen_ms = 0;

    // 1. Transmission Temp (PGN_TEMPS -> trans_temp)
    if (receiver.getLastPayload(PGN_TEMPS, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_temps_t, trans_temp) + sizeof(pgn_temps_t::trans_temp);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_temps_t *>(buf);
            s_trans_lerp = lerp_f(s_trans_lerp, static_cast<float>(msg->trans_temp), LERP_ALPHA);
        }
    }

    // 2. Fuel Pressure (PGN_PRESSURES -> fuel_pressure)
    if (receiver.getLastPayload(PGN_PRESSURES, buf, &len, &last_seen_ms) == ESP_OK)
    {
        constexpr size_t min_len = offsetof(pgn_pressures_t, fuel_pressure) + sizeof(pgn_pressures_t::fuel_pressure);
        if (len >= min_len)
        {
            const auto *msg = reinterpret_cast<const pgn_pressures_t *>(buf);
            s_fuel_lerp = lerp_f(s_fuel_lerp, static_cast<float>(msg->fuel_pressure), LERP_ALPHA);
        }
    }
}

/**
 * @brief Step 2: Apply smoothed values directly to LVGL widgets.
 * @note Call this INSIDE the lvgl_lock() window.
 */
extern "C" void small_gauge_draw(void)
{
    update_trans_temp_meter(static_cast<int32_t>(s_trans_lerp));
    update_fuel_pressure_meter(static_cast<int32_t>(s_fuel_lerp));
}