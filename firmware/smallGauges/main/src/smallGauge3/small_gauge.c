// small_gauge.c
// Gauge module that reads Transmission Temp and Fuel Pressure from VCAN PGNs
// - transTemp <- PGN_TEMPS   offset TEMP_TRANS_OFFSET (trans_temp)
// - fuelPressure <- PGN_PRESSURES offset PRESSURE_FUEL_OFFSET (fuel_pressure)
//
// Exposes small_gauge_init/deinit/draw (same public API as other variants)

#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"        // update_trans_temp_meter, update_fuel_pressure_meter
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "small_gauge";

/* Local state kept by this gauge module. */
typedef struct {
    int32_t trans_temp;     // transmission temp (raw)
    int32_t fuel_pressure;  // fuel pressure (raw)
    uint32_t last_update_ms;
} sg2_state_t;

static sg2_state_t s_state;
static SemaphoreHandle_t s_mutex = NULL;

/* Smoothing state (module-local) */
static float s_trans_lerp = 0.0f;
static float s_fuel_lerp = 0.0f;
static const float LERP_ALPHA = 0.15f;

/* Helpers */
static inline int16_t read_s16_le(const uint8_t *b) { return (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8)); }
static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000ULL); }
static inline float lerp_f(float a, float b, float t) { return a + (b - a) * t; }

/* Forward declarations for callbacks */
static void cb_temps(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);
static void cb_pressures(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);

/* Initialize module: create mutex, register PGN callbacks, and prime state from last payloads */
void small_gauge_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof(s_state));
    s_state.last_update_ms = 0;

    /* Register callbacks for the PGNs we need */
    vcan_receiver_register_pgn(PGN_TEMPS, cb_temps, NULL);       // trans temp at TEMP_TRANS_OFFSET
    vcan_receiver_register_pgn(PGN_PRESSURES, cb_pressures, NULL); // fuel pressure at PRESSURE_FUEL_OFFSET

    /* Prime initial values from last-known payloads (optional) */
    uint8_t buf[8];
    uint8_t len;
    uint32_t seen;

    if (vcan_receiver_get_last_payload(PGN_TEMPS, buf, &len, &seen) == ESP_OK &&
        len >= (TEMP_TRANS_OFFSET + 2)) {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            int16_t t_raw = read_s16_le(&buf[TEMP_TRANS_OFFSET]);
            s_state.trans_temp = (int32_t)t_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed TRANS bytes=%02X %02X -> %d", buf[TEMP_TRANS_OFFSET], buf[TEMP_TRANS_OFFSET + 1], t_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan_receiver_get_last_payload(PGN_PRESSURES, buf, &len, &seen) == ESP_OK &&
        len >= (PRESSURE_FUEL_OFFSET + 2)) {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            int16_t f_raw = read_s16_le(&buf[PRESSURE_FUEL_OFFSET]);
            s_state.fuel_pressure = (int32_t)f_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed FUEL bytes=%02X %02X -> %d", buf[PRESSURE_FUEL_OFFSET], buf[PRESSURE_FUEL_OFFSET + 1], f_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    ESP_LOGI(TAG, "initialized");
}

/* Unregister callbacks and free resources */
void small_gauge_deinit(void)
{
    vcan_receiver_unregister_pgn(PGN_TEMPS, cb_temps, NULL);
    vcan_receiver_unregister_pgn(PGN_PRESSURES, cb_pressures, NULL);

    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    ESP_LOGI(TAG, "deinitialized");
}

/* Draw function: called from UI task with LVGL locked.
   Reads local state, applies smoothing, and calls the UI update helpers. */
void small_gauge_draw(void)
{
    int32_t trans_raw = 0, fuel_raw = 0;
    uint32_t last_ms = 0;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return;
    trans_raw = s_state.trans_temp;
    fuel_raw = s_state.fuel_pressure;
    last_ms = s_state.last_update_ms;
    xSemaphoreGive(s_mutex);

    /* Apply smoothing locally */
    s_trans_lerp = lerp_f(s_trans_lerp, (float)trans_raw, LERP_ALPHA);
    s_fuel_lerp  = lerp_f(s_fuel_lerp,  (float)fuel_raw,  LERP_ALPHA);

    /* Call UI update functions (caller must hold LVGL lock) */
    update_trans_temp_meter((int32_t)s_trans_lerp);
    update_fuel_pressure_meter((int32_t)s_fuel_lerp);

    /* Optional per-gauge stale indicator (example 2000 ms) */
    if ((now_ms() - last_ms) > 2000) {
        // show per-gauge stale indicator if you have one
    }
}

/* Callback implementations: decode only the bytes we need and update local state.
   These are called by vcan_receiver worker task (not in esp_now RX path). Keep them short. */

static void cb_temps(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (TEMP_TRANS_OFFSET + 2)) return;
    int16_t t_raw = read_s16_le(&payload[TEMP_TRANS_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.trans_temp = (int32_t)t_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_pressures(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (PRESSURE_FUEL_OFFSET + 2)) return;
    int16_t f_raw = read_s16_le(&payload[PRESSURE_FUEL_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.fuel_pressure = (int32_t)f_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}
