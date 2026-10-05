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

struct sg_state_t {
    int32_t trans_temp = 0;     
    int32_t fuel_pressure = 0;  
    uint32_t last_update_ms = 0;
};

static sg_state_t s_state;
static SemaphoreHandle_t s_mutex = nullptr;

static float s_trans_lerp = 0.0f;
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
    s_state = sg_state_t{};

    // 1. PGN_TEMPS -> msg->trans_temp
    vcan::Receiver::instance().registerCallback(PGN_TEMPS, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
        if (!payload || len < sizeof(pgn_temps_t)) return;
        const auto *msg = reinterpret_cast<const pgn_temps_t*>(payload);
        if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
            s_state.trans_temp = static_cast<int32_t>(msg->trans_temp);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    });

    // 2. PGN_PRESSURES -> msg->fuel_pressure
    vcan::Receiver::instance().registerCallback(PGN_PRESSURES, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
        if (!payload || len < sizeof(pgn_pressures_t)) return;
        const auto *msg = reinterpret_cast<const pgn_pressures_t*>(payload);
        if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
            s_state.fuel_pressure = static_cast<int32_t>(msg->fuel_pressure);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    });

    // Prime initial values from Receiver history
    uint8_t buf[32];
    uint8_t len = 0;
    uint32_t seen = 0;

    if (vcan::Receiver::instance().getLastPayload(PGN_TEMPS, buf, &len, &seen) == ESP_OK && len >= sizeof(pgn_temps_t)) {
        const auto *msg = reinterpret_cast<const pgn_temps_t*>(buf);
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            s_state.trans_temp = static_cast<int32_t>(msg->trans_temp);
            s_state.last_update_ms = now_ms();
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan::Receiver::instance().getLastPayload(PGN_PRESSURES, buf, &len, &seen) == ESP_OK && len >= sizeof(pgn_pressures_t)) {
        const auto *msg = reinterpret_cast<const pgn_pressures_t*>(buf);
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            s_state.fuel_pressure = static_cast<int32_t>(msg->fuel_pressure);
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
    int32_t trans_raw = 0, fuel_raw = 0;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return;
    trans_raw = s_state.trans_temp;
    fuel_raw = s_state.fuel_pressure;
    xSemaphoreGive(s_mutex);

    s_trans_lerp = lerp_f(s_trans_lerp, static_cast<float>(trans_raw), LERP_ALPHA);
    s_fuel_lerp  = lerp_f(s_fuel_lerp,  static_cast<float>(fuel_raw),  LERP_ALPHA);

    update_trans_temp_meter(static_cast<int32_t>(s_trans_lerp));
    update_fuel_pressure_meter(static_cast<int32_t>(s_fuel_lerp));
}