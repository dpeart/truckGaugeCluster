// small_gauge2.c
// Gauge module that reads IAT, EGT, and battery level from VCAN PGNs
// - IAT  <- PGN_TEMPS   offset TEMP_IAT_OFFSET (ia_temp)
// - EGT  <- PGN_EXHAUST_DIG offset EXHAUST_EGT_OFFSET (eg_temp)
// - Batt <- PGN_PRESSURES  offset PRESSURE_BATTERY_OFFSET (battery_level)
//
// This is a copy/variant of small_gauge1.c adapted to call:
//   update_iat_meter(int32_t)
//   update_egt_meter(int32_t)
//   update_battery_arc(int32_t)

#include "small_gauge.h"
#include "vcan_receiver.h"
#include "vcan_protocol.h"   // for PGN constants and TEMP_*_OFFSET defines
#include "updateUI.h"        // for update_iat_meter, update_egt_meter, update_battery_arc
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "small_gauge";

/* Local state kept by this gauge module. */
typedef struct {
    int32_t iat;        // intake air temp (raw)
    int32_t egt;        // exhaust gas temp (raw)
    int32_t battery;    // battery level (raw)
    uint32_t last_update_ms;
} sg2_state_t;

static sg2_state_t s_state;
static SemaphoreHandle_t s_mutex = NULL;

/* Smoothing state (module-local) */
static float s_iat_lerp = 0.0f;
static float s_egt_lerp = 0.0f;
static float s_batt_lerp = 0.0f;
static const float LERP_ALPHA = 0.15f;

/* Helpers */
static inline int16_t read_s16_le(const uint8_t *b) { return (int16_t)(b[0] | (b[1] << 8)); }
static inline int32_t read_s32_le(const uint8_t *b) { return (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)); }
static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000ULL); }
static inline float lerp_f(float a, float b, float t) { return a + (b - a) * t; }

/* Forward declarations for callbacks */
static void cb_temps(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);
static void cb_exhaust_dig(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);
static void cb_pressures(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);

/* Initialize module: create mutex, register PGN callbacks, and prime state from last payloads */
void small_gauge_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof(s_state));
    s_state.last_update_ms = 0;

    /* Register callbacks for the PGNs we need */
    vcan_receiver_register_pgn(PGN_TEMPS, cb_temps, NULL);             // IAT at TEMP_IAT_OFFSET
    vcan_receiver_register_pgn(PGN_EXHAUST_DIG, cb_exhaust_dig, NULL); // EGT at EXHAUST_EGT_OFFSET
    vcan_receiver_register_pgn(PGN_PRESSURES, cb_pressures, NULL);     // battery at PRESSURE_BATTERY_OFFSET

    /* Prime initial values from last-known payloads (optional) */
    uint8_t buf[8];
    uint8_t len;
    uint32_t seen;

    if (vcan_receiver_get_last_payload(PGN_TEMPS, buf, &len, &seen) == ESP_OK &&
        len >= (TEMP_IAT_OFFSET + 2)) {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            int16_t iat_raw = read_s16_le(&buf[TEMP_IAT_OFFSET]);
            s_state.iat = (int32_t)iat_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed IAT bytes=%02X %02X -> %d", buf[TEMP_IAT_OFFSET], buf[TEMP_IAT_OFFSET + 1], iat_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan_receiver_get_last_payload(PGN_EXHAUST_DIG, buf, &len, &seen) == ESP_OK &&
        len >= (EXHAUST_EGT_OFFSET + 2)) {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            int32_t egt_raw = read_s32_le(&buf[EXHAUST_EGT_OFFSET]);
            s_state.egt = (int32_t)egt_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed EGT bytes=%02X %02X -> %d", buf[EXHAUST_EGT_OFFSET], buf[EXHAUST_EGT_OFFSET + 1], egt_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan_receiver_get_last_payload(PGN_PRESSURES, buf, &len, &seen) == ESP_OK &&
        len >= (PRESSURE_BATTERY_OFFSET + 2)) {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            int16_t batt_raw = read_s16_le(&buf[PRESSURE_BATTERY_OFFSET]);
            s_state.battery = (int32_t)batt_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed BATT bytes=%02X %02X -> %d", buf[PRESSURE_BATTERY_OFFSET], buf[PRESSURE_BATTERY_OFFSET + 1], batt_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    ESP_LOGI(TAG, "initialized");
}

/* Unregister callbacks and free resources */
void small_gauge_deinit(void)
{
    vcan_receiver_unregister_pgn(PGN_TEMPS, cb_temps, NULL);
    vcan_receiver_unregister_pgn(PGN_EXHAUST_DIG, cb_exhaust_dig, NULL);
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
    int32_t iat_raw = 0, egt_raw = 0, batt_raw = 0;
    uint32_t last_ms = 0;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return;
    iat_raw = s_state.iat;
    egt_raw = s_state.egt;
    batt_raw = s_state.battery;
    last_ms = s_state.last_update_ms;
    xSemaphoreGive(s_mutex);

    /* Apply smoothing locally */
    s_iat_lerp  = lerp_f(s_iat_lerp,  (float)iat_raw, LERP_ALPHA);
    s_egt_lerp  = lerp_f(s_egt_lerp,  (float)egt_raw, LERP_ALPHA);
    s_batt_lerp = lerp_f(s_batt_lerp, (float)batt_raw, LERP_ALPHA);

    /* Call UI update functions (caller must hold LVGL lock) */
    update_iat_meter((int32_t)s_iat_lerp);
    update_egt_meter((int32_t)s_egt_lerp);
    update_battery_arc((int32_t)s_batt_lerp);

    /* Optional per-gauge stale indicator (example 2000 ms) */
    if ((now_ms() - last_ms) > 2000) {
        // show per-gauge stale indicator if you have one
    }
}

/* Callback implementations: decode only the bytes we need and update local state.
   These are called by vcan_receiver worker task (not in esp_now RX path). Keep them short. */

static void cb_temps(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    // Expecting at least 2 bytes at TEMP_IAT_OFFSET (ia_temp)
    if (len < (TEMP_IAT_OFFSET + 2)) return;
    int16_t iat_raw = read_s16_le(&payload[TEMP_IAT_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.iat = (int32_t)iat_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_exhaust_dig(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    // Expecting at least 2 bytes at EXHAUST_EGT_OFFSET (eg_temp)
    if (len < (EXHAUST_EGT_OFFSET + 2)) return;
    int16_t egt_raw = read_s16_le(&payload[EXHAUST_EGT_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.egt = (int32_t)egt_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_pressures(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    // Expecting at least 2 bytes at PRESSURE_BATTERY_OFFSET (battery_level)
    if (len < (PRESSURE_BATTERY_OFFSET + 2)) return;
    int16_t batt_raw = read_s16_le(&payload[PRESSURE_BATTERY_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.battery = (int32_t)batt_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}
