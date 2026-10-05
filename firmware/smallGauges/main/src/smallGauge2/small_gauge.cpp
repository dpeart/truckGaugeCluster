#include "small_gauge.h"
#include "esp_log.h"
#include "vcan_receiver.h"
#include "vcan_protocol.h"
#include "updateUI.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <cstring>
#include <cmath>

static const char *TAG = "small_gauge";

struct sg2_state_t {
    int32_t iat = 0;
    int32_t egt = 0;
    int32_t battery = 0;
    uint32_t last_update_ms = 0;
};

static sg2_state_t s_state;
static SemaphoreHandle_t s_mutex = nullptr;

static float s_iat_lerp = 0.0f;
static float s_egt_lerp = 0.0f;
static float s_batt_lerp = 0.0f;
static constexpr float LERP_ALPHA = 0.15f;

static inline uint32_t now_ms() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static inline float lerp_f(float a, float b, float t) {
    return a + (b - a) * t;
}

extern "C" void small_gauge_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    s_state = sg2_state_t{};

    // 1. PGN_TEMPS -> msg->ia_temp
    vcan::Receiver::instance().registerCallback(PGN_TEMPS, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
        if (!payload || len < sizeof(pgn_temps_t)) return;
        const auto *msg = reinterpret_cast<const pgn_temps_t*>(payload);
        if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
            s_state.iat = static_cast<int32_t>(msg->ia_temp);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    });

    // 2. PGN_EXHAUST_DIG -> msg->eg_temp (int32_t)
    vcan::Receiver::instance().registerCallback(PGN_EXHAUST_DIG, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
        if (!payload || len < sizeof(pgn_exhaust_dig_t)) return;
        const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t*>(payload);
        if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
            s_state.egt = msg->eg_temp;
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    });

    // 3. PGN_PRESSURES -> msg->battery_level
    vcan::Receiver::instance().registerCallback(PGN_PRESSURES, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
        if (!payload || len < sizeof(pgn_pressures_t)) return;
        const auto *msg = reinterpret_cast<const pgn_pressures_t*>(payload);
        if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
            s_state.battery = static_cast<int32_t>(msg->battery_level);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    });

    // Prime initial values directly from Receiver
    uint8_t buf[32];
    uint8_t len = 0;
    uint32_t seen = 0;

    if (vcan::Receiver::instance().getLastPayload(PGN_TEMPS, buf, &len, &seen) == ESP_OK && len >= sizeof(pgn_temps_t)) {
        const auto *msg = reinterpret_cast<const pgn_temps_t*>(buf);
        s_state.iat = static_cast<int32_t>(msg->ia_temp);
        s_state.last_update_ms = now_ms();
    }

    if (vcan::Receiver::instance().getLastPayload(PGN_EXHAUST_DIG, buf, &len, &seen) == ESP_OK && len >= sizeof(pgn_exhaust_dig_t)) {
        const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t*>(buf);
        s_state.egt = msg->eg_temp;
        s_state.last_update_ms = now_ms();
    }

    if (vcan::Receiver::instance().getLastPayload(PGN_PRESSURES, buf, &len, &seen) == ESP_OK && len >= sizeof(pgn_pressures_t)) {
        const auto *msg = reinterpret_cast<const pgn_pressures_t*>(buf);
        s_state.battery = static_cast<int32_t>(msg->battery_level);
        s_state.last_update_ms = now_ms();
    }

    ESP_LOGI(TAG, "small_gauge C++ initialized");
}

extern "C" void small_gauge_deinit(void)
{
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = nullptr;
    }
    ESP_LOGI(TAG, "deinitialized");
}

extern "C" void small_gauge_draw(void)
{
    int32_t iat_raw = 0, egt_raw = 0, batt_raw = 0;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return;
    iat_raw = s_state.iat;
    egt_raw = s_state.egt;
    batt_raw = s_state.battery;
    xSemaphoreGive(s_mutex);

    s_iat_lerp  = lerp_f(s_iat_lerp,  static_cast<float>(iat_raw), LERP_ALPHA);
    s_egt_lerp  = lerp_f(s_egt_lerp,  static_cast<float>(egt_raw), LERP_ALPHA);
    s_batt_lerp = lerp_f(s_batt_lerp, static_cast<float>(batt_raw), LERP_ALPHA);

    update_iat_meter(static_cast<int32_t>(s_iat_lerp));
    update_egt_meter(static_cast<int32_t>(s_egt_lerp));
    update_battery_arc(static_cast<int32_t>(s_batt_lerp));
}