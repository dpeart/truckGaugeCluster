#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <cstring>
#include <cmath>

static const char *TAG = "small_gauge";

struct sg1_state_t {
    int32_t coolant = 0;      
    int32_t oil_pressure = 0; 
    int32_t fuel_level = 0;   
    uint32_t last_update_ms = 0;
};

static sg1_state_t s_state;
static SemaphoreHandle_t s_mutex = nullptr;

static float s_coolant_lerp = 0.0f;
static float s_oil_lerp = 0.0f;
static float s_fuel_lerp = 0.0f;
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
    s_state = sg1_state_t{};

    // 1. PGN_ENGINE_CORE -> msg->coolant_temp
    vcan::Receiver::instance().registerCallback(PGN_ENGINE_CORE, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
        if (!payload || len < sizeof(pgn_engine_core_t)) return;
        const auto *msg = reinterpret_cast<const pgn_engine_core_t*>(payload);
        if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
            s_state.coolant = static_cast<int32_t>(msg->coolant_temp);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    });

    // 2. PGN_PRESSURES -> msg->oil_pressure
    vcan::Receiver::instance().registerCallback(PGN_PRESSURES, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
        if (!payload || len < sizeof(pgn_pressures_t)) return;
        const auto *msg = reinterpret_cast<const pgn_pressures_t*>(payload);
        if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
            s_state.oil_pressure = static_cast<int32_t>(msg->oil_pressure);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    });

    // 3. PGN_EXHAUST_DIG -> msg->fuel_level
    vcan::Receiver::instance().registerCallback(PGN_EXHAUST_DIG, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
        if (!payload || len < sizeof(pgn_exhaust_dig_t)) return;
        const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t*>(payload);
        if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
            s_state.fuel_level = static_cast<int32_t>(msg->fuel_level);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    });

    // Prime initial values from Receiver history
    uint8_t buf[32];
    uint8_t len = 0;
    uint32_t seen = 0;

    if (vcan::Receiver::instance().getLastPayload(PGN_ENGINE_CORE, buf, &len, &seen) == ESP_OK && len >= sizeof(pgn_engine_core_t)) {
        const auto *msg = reinterpret_cast<const pgn_engine_core_t*>(buf);
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            s_state.coolant = static_cast<int32_t>(msg->coolant_temp);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan::Receiver::instance().getLastPayload(PGN_PRESSURES, buf, &len, &seen) == ESP_OK && len >= sizeof(pgn_pressures_t)) {
        const auto *msg = reinterpret_cast<const pgn_pressures_t*>(buf);
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            s_state.oil_pressure = static_cast<int32_t>(msg->oil_pressure);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan::Receiver::instance().getLastPayload(PGN_EXHAUST_DIG, buf, &len, &seen) == ESP_OK && len >= sizeof(pgn_exhaust_dig_t)) {
        const auto *msg = reinterpret_cast<const pgn_exhaust_dig_t*>(buf);
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            s_state.fuel_level = static_cast<int32_t>(msg->fuel_level);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    }

    ESP_LOGI(TAG, "initialized small_gauge");
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
    int32_t coolant_raw = 0, oil_raw = 0, fuel_raw = 0;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return;
    coolant_raw = s_state.coolant;
    oil_raw = s_state.oil_pressure;
    fuel_raw = s_state.fuel_level;
    xSemaphoreGive(s_mutex);

    s_coolant_lerp = lerp_f(s_coolant_lerp, static_cast<float>(coolant_raw), LERP_ALPHA);
    s_oil_lerp     = lerp_f(s_oil_lerp,     static_cast<float>(oil_raw),     LERP_ALPHA);
    s_fuel_lerp    = lerp_f(s_fuel_lerp,    static_cast<float>(fuel_raw),    LERP_ALPHA);

    update_coolant_meter(static_cast<int32_t>(s_coolant_lerp));
    update_oil_pressure_meter(static_cast<int32_t>(s_oil_lerp));
    update_fuel_arc(static_cast<int32_t>(s_fuel_lerp));
}