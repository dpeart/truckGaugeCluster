// small_gauge.c
// Minimal gauge module that supports boost pressure and oil temp
// - boost pressure <- PGN_PRESSURES offset PRESSURE_BOOST_OFFSET (int16 LE)
// - oil temp       <- PGN_ENGINE_CORE offset ENGINE_COOLANT_OFFSET (int16 LE)
//
// Exposes small_gauge_init/deinit/draw (public API)

#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"        // update_oil_temp_meter(int32_t), update_boost_pressure_meter(int32_t)
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "small_gauge";

/* Local state kept by this gauge module. */
typedef struct
{
    int32_t oil_temp;       // oil/coolant temp (raw)
    int32_t boost_pressure; // boost pressure (raw)
    uint32_t last_update_ms;
} sg_state_t;

static sg_state_t s_state;
static SemaphoreHandle_t s_mutex = NULL;

/* Smoothing state (module-local) */
static float s_oil_lerp = 0.0f;
static float s_boost_lerp = 0.0f;
static const float LERP_ALPHA = 0.15f;

/* Helpers */
static inline int16_t read_s16_le(const uint8_t *b) { return (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8)); }
static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000ULL); }

/* Forward declarations for callbacks */
static void cb_engine_core(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);
static void cb_pressures(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx);

/* Initialize module: create mutex, register PGN callbacks, and prime state from last payloads */
void small_gauge_init(void)
{
    if (!s_mutex)
        s_mutex = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof(s_state));
    s_state.last_update_ms = 0;
    s_state.oil_temp = 0;
    s_state.boost_pressure = 0;

    /* Register callbacks for the PGNs we need */
    vcan_receiver_register_pgn(PGN_ENGINE_CORE, cb_engine_core, NULL); // oil temp at ENGINE_COOLANT_OFFSET
    vcan_receiver_register_pgn(PGN_PRESSURES, cb_pressures, NULL);     // boost pressure at PRESSURE_BOOST_OFFSET

    /* Prime initial values from last-known payloads (optional) */
    uint8_t buf[8];
    uint8_t len;
    uint32_t seen;

    if (vcan_receiver_get_last_payload(PGN_ENGINE_CORE, buf, &len, &seen) == ESP_OK &&
        len >= (ENGINE_COOLANT_OFFSET + 2))
    {
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE)
        {
            int16_t oil_raw = read_s16_le(&buf[ENGINE_COOLANT_OFFSET]);
            s_state.oil_temp = (int32_t)oil_raw;
            s_state.last_update_ms = now_ms();
            ESP_LOGI(TAG, "primed OIL bytes=%02X %02X -> %d", buf[ENGINE_COOLANT_OFFSET], buf[ENGINE_COOLANT_OFFSET + 1], oil_raw);
            xSemaphoreGive(s_mutex);
        }
    }

    if (vcan_receiver_get_last_payload(PGN_PRESSURES, buf, &len, &seen) == ESP_OK)
    {
        if (len >= (PRESSURE_BOOST_OFFSET + 2))
        {
            if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE)
            {
                int16_t b_raw = read_s16_le(&buf[PRESSURE_BOOST_OFFSET]);
                s_state.boost_pressure = (int32_t)b_raw;
                s_state.last_update_ms = now_ms();
                ESP_LOGI(TAG, "primed BOOST bytes=%02X %02X -> %d", buf[PRESSURE_BOOST_OFFSET], buf[PRESSURE_BOOST_OFFSET + 1], b_raw);
                xSemaphoreGive(s_mutex);
            }
        }
    }

    ESP_LOGI(TAG, "initialized (boost+oil)");
}

/* Unregister callbacks and free resources */
void small_gauge_deinit(void)
{
    vcan_receiver_unregister_pgn(PGN_ENGINE_CORE, cb_engine_core, NULL);
    vcan_receiver_unregister_pgn(PGN_PRESSURES, cb_pressures, NULL);

    if (s_mutex)
    {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    ESP_LOGI(TAG, "deinitialized");
}

/* Draw function: called from UI task with LVGL locked.
   Reads local state, applies smoothing, and calls the UI update helpers. */
void small_gauge_draw(void)
{
    int32_t oil_raw = 0;
    int32_t boost_raw = 0;
    uint32_t last_ms = 0;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE)
        return;
    oil_raw = s_state.oil_temp;
    boost_raw = s_state.boost_pressure;
    last_ms = s_state.last_update_ms;
    xSemaphoreGive(s_mutex);

    /* Apply smoothing locally for oil temp and boost pressure */
    s_oil_lerp = s_oil_lerp + ((float)oil_raw - s_oil_lerp) * LERP_ALPHA;
    s_boost_lerp = s_boost_lerp + ((float)boost_raw - s_boost_lerp) * LERP_ALPHA;

    /* Call UI update functions (caller must hold LVGL lock) */
    update_oil_temp_meter((int32_t)s_oil_lerp);
    update_boost_pressure_meter((int32_t)s_boost_lerp);

    /* Optional stale indicator */
    if ((now_ms() - last_ms) > 2000)
    {
        // show stale indicator if available
    }
}

/* Callback implementations: decode only the bytes we need and update local state.
   These are called by vcan_receiver worker task. Keep them short. */

static void cb_engine_core(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (ENGINE_COOLANT_OFFSET + 2))
        return;
    int16_t oil_raw = read_s16_le(&payload[ENGINE_COOLANT_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        s_state.oil_temp = (int32_t)oil_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}

static void cb_pressures(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (PRESSURE_BOOST_OFFSET + 2))
        return;
    int16_t b_raw = read_s16_le(&payload[PRESSURE_BOOST_OFFSET]);
    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        s_state.boost_pressure = (int32_t)b_raw;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
}
