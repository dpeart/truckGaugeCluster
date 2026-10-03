#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "small_gauge1";

/* Local state kept by this gauge module.
   Only fields required by the UI are stored. */
typedef struct {
    int32_t coolant;      // coolantTemp (raw)
    int32_t oil_pressure; // oilPressure (raw)
    int32_t fuel_level;   // fuelLevel (raw)
    uint32_t last_update_ms;
} sg1_state_t;

static sg1_state_t s_state;
static SemaphoreHandle_t s_mutex = NULL;

/* Smoothing state (module-local) */
static float s_coolant_lerp = 0.0f;
static float s_oil_lerp = 0.0f;
static float s_fuel_lerp = 0.0f;
static const float LERP_ALPHA = 0.15f;

/* Helpers */
static inline int16_t read_s16_le(const uint8_t *b) { return (int16_t)(b[0] | (b[1] << 8)); }
static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000ULL); }
static inline float lerp_f(float a, float b, float t) { return a + (b - a) * t; }

/* Forward declarations for callbacks */
static void cb_engine_core(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);
static void cb_pressures(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);
static void cb_exhaust_dig(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);

/* Initialize module: create mutex, register PGN callbacks, and prime state from last payloads */
void small_gauge_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof(s_state));
    s_state.last_update_ms = 0;

    /* Register callbacks for only the PGNs we need */
    vcan_receiver_register_pgn(PGN_ENGINE_CORE, cb_engine_core, NULL);
    vcan_receiver_register_pgn(PGN_PRESSURES, cb_pressures, NULL);
    vcan_receiver_register_pgn(PGN_EXHAUST_DIG, cb_exhaust_dig, NULL);

    /* Prime initial values from last-known payloads (optional) */
    uint8_t buf[8];
    uint8_t len;
    uint32_t seen;

    if (vcan_receiver_get_last_payload(PGN_ENGINE_CORE, buf, &len, &seen) == ESP_OK &&
        len >= (ENGINE_COOLANT_OFFSET + 2)) {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            int16_t coolant_raw = read_s16_le(&buf[ENGINE_COOLANT_OFFSET]);
            s_state.coolant = (int32_t)coolant_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed COOLANT bytes=%02X %02X -> %d", buf[ENGINE_COOLANT_OFFSET], buf[ENGINE_COOLANT_OFFSET + 1], coolant_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan_receiver_get_last_payload(PGN_PRESSURES, buf, &len, &seen) == ESP_OK &&
        len >= (PRESSURE_OIL_OFFSET + 2)) {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            int16_t oil_raw = read_s16_le(&buf[PRESSURE_OIL_OFFSET]);
            s_state.oil_pressure = (int32_t)oil_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed OIL bytes=%02X %02X -> %d", buf[PRESSURE_OIL_OFFSET], buf[PRESSURE_OIL_OFFSET + 1], oil_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan_receiver_get_last_payload(PGN_EXHAUST_DIG, buf, &len, &seen) == ESP_OK &&
        len >= (EXHAUST_FUEL_OFFSET + 2)) {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            int16_t fuel_raw = read_s16_le(&buf[EXHAUST_FUEL_OFFSET]);
            s_state.fuel_level = (int32_t)fuel_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed FUEL bytes=%02X %02X -> %d", buf[EXHAUST_FUEL_OFFSET], buf[EXHAUST_FUEL_OFFSET + 1], fuel_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    ESP_LOGI(TAG, "initialized");
}

/* Unregister callbacks and free resources */
void small_gauge_deinit(void)
{
    vcan_receiver_unregister_pgn(PGN_ENGINE_CORE, cb_engine_core, NULL);
    vcan_receiver_unregister_pgn(PGN_PRESSURES, cb_pressures, NULL);
    vcan_receiver_unregister_pgn(PGN_EXHAUST_DIG, cb_exhaust_dig, NULL);

    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    ESP_LOGI(TAG, "deinitialized");
}

/* Draw function: called from UI task with LVGL locked.
   It reads the small local state, applies smoothing, and calls the UI update helpers. */
void small_gauge_draw(void)
{
    int32_t coolant_raw = 0, oil_raw = 0, fuel_raw = 0;
    uint32_t last_ms = 0;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return;
    coolant_raw = s_state.coolant;
    oil_raw = s_state.oil_pressure;
    fuel_raw = s_state.fuel_level;
    last_ms = s_state.last_update_ms;
    xSemaphoreGive(s_mutex);

    /* Apply smoothing locally */
    s_coolant_lerp = lerp_f(s_coolant_lerp, (float)coolant_raw, LERP_ALPHA);
    s_oil_lerp     = lerp_f(s_oil_lerp,     (float)oil_raw,     LERP_ALPHA);
    s_fuel_lerp    = lerp_f(s_fuel_lerp,    (float)fuel_raw,    LERP_ALPHA);

    /* Call UI update functions (caller must hold LVGL lock) */
    update_coolant_meter((int32_t)s_coolant_lerp);
    update_oil_pressure_meter((int32_t)s_oil_lerp);
    update_fuel_arc((int32_t)s_fuel_lerp);

    /* Optional per-gauge stale indicator (example 2000 ms) */
    if ((now_ms() - last_ms) > 2000) {
        // show per-gauge stale indicator if you have one
    }
}

/* Callback implementations: decode only the bytes we need and update local state.
   These are called by vcan_receiver worker task (not in esp_now RX path). Keep them short. */

static void cb_engine_core(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (ENGINE_COOLANT_OFFSET + 2)) return;
    int16_t coolant_raw = read_s16_le(&payload[ENGINE_COOLANT_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.coolant = (int32_t)coolant_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_pressures(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (PRESSURE_OIL_OFFSET + 2)) return;
    int16_t oil_raw = read_s16_le(&payload[PRESSURE_OIL_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.oil_pressure = (int32_t)oil_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_exhaust_dig(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (EXHAUST_FUEL_OFFSET + 2)) return;
    int16_t fuel_raw = read_s16_le(&payload[EXHAUST_FUEL_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.fuel_level = (int32_t)fuel_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}
