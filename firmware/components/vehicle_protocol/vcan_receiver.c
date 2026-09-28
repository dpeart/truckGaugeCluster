#include "vcan_receiver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "VCAN_RX";

// Helpers: wire-format is little-endian for id and multi-byte fields.
static inline uint16_t read_u16_le(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static inline uint32_t read_u32_le(const uint8_t *b) { return (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)); }
static inline int16_t read_s16_le(const uint8_t *b) { return (int16_t)read_u16_le(b); }
static inline int32_t read_s32_le(const uint8_t *b) { return (int32_t)read_u32_le(b); }

// ID helpers (must match vcan_build_id in transmitter)
static inline uint8_t id_get_priority(uint32_t id) { return (uint8_t)((id >> 26) & 0x07); }
static inline uint16_t id_get_pgn(uint32_t id) { return (uint16_t)((id >> 8) & 0xFFFF); }
static inline uint8_t id_get_src(uint32_t id) { return (uint8_t)(id & 0xFF); }

// Configurable limits
#define VCAN_MAX_SUBS           64   // total subscription entries
#define VCAN_MAX_TRACKED_PGNS   64   // how many PGNs we keep last-payload for
#define VCAN_EVENT_QUEUE_LEN    128  // event queue depth
#define VCAN_WORKER_TASK_NAME   "vcan_rx_worker"
#define VCAN_WORKER_STACK       4096
#define VCAN_WORKER_PRIO        5

typedef struct {
    uint16_t pgn;
    vcan_pgn_callback_t cb;
    void *user_ctx;
} sub_entry_t;

typedef struct {
    uint16_t pgn;
    uint8_t  data[8];
    uint8_t  len;
    uint32_t last_seen_ms;
} tracked_pgn_t;

typedef struct {
    uint16_t pgn;
    uint8_t  src;
    uint8_t  len;
    uint8_t  data[8];
} vcan_event_t;

// State
static SemaphoreHandle_t s_mutex = NULL;
static sub_entry_t s_subs[VCAN_MAX_SUBS];
static size_t s_sub_count = 0;
static tracked_pgn_t s_tracked[VCAN_MAX_TRACKED_PGNS];
static size_t s_tracked_count = 0;
static QueueHandle_t s_event_q = NULL;
static TaskHandle_t s_worker_task = NULL;
static uint8_t s_last_heartbeat_seq = 0xFF;
static uint32_t s_last_heartbeat_ms = 0;

// Internal helpers
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000ULL); }

static void ensure_init_resources(void) {
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    if (!s_event_q) s_event_q = xQueueCreate(VCAN_EVENT_QUEUE_LEN, sizeof(vcan_event_t));
}

// Find or create tracked PGN entry; returns pointer or NULL if no space
static tracked_pgn_t *tracked_find_or_create(uint16_t pgn) {
    for (size_t i = 0; i < s_tracked_count; ++i) {
        if (s_tracked[i].pgn == pgn) return &s_tracked[i];
    }
    if (s_tracked_count < VCAN_MAX_TRACKED_PGNS) {
        tracked_pgn_t *t = &s_tracked[s_tracked_count++];
        t->pgn = pgn;
        t->len = 0;
        t->last_seen_ms = 0;
        memset(t->data, 0, sizeof(t->data));
        return t;
    }
    return NULL;
}

// Update tracked PGN payload and timestamp
static void tracked_update(uint16_t pgn, const uint8_t *payload, uint8_t len) {
    tracked_pgn_t *t = tracked_find_or_create(pgn);
    if (!t) return;
    t->len = (len > 8) ? 8 : len;
    if (t->len) memcpy(t->data, payload, t->len);
    t->last_seen_ms = now_ms();
}

// Dispatch event to registered callbacks (called from worker task)
static void dispatch_event(const vcan_event_t *ev) {
    // copy subs under mutex to avoid holding mutex while calling callbacks
    sub_entry_t subs_copy[VCAN_MAX_SUBS];
    size_t subs_n = 0;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < s_sub_count; ++i) {
            if (s_subs[i].pgn == ev->pgn) {
                subs_copy[subs_n++] = s_subs[i];
                if (subs_n >= VCAN_MAX_SUBS) break;
            }
        }
        xSemaphoreGive(s_mutex);
    }

    for (size_t i = 0; i < subs_n; ++i) {
        if (subs_copy[i].cb) {
            // callbacks must be non-blocking; if they block, consider queueing work
            subs_copy[i].cb(ev->pgn, ev->src, ev->data, ev->len, subs_copy[i].user_ctx);
        }
    }
}

// Worker task: pulls events and dispatches
static void vcan_worker_task(void *arg) {
    (void)arg;
    vcan_event_t ev;
    while (1) {
        if (xQueueReceive(s_event_q, &ev, portMAX_DELAY) == pdTRUE) {
            // update tracked payload and heartbeat handling
            if (ev.pgn == PGN_HEARTBEAT) {
                if (ev.len >= 1) {
                    s_last_heartbeat_seq = ev.data[0];
                    s_last_heartbeat_ms = now_ms();
                }
            }
            // update tracked payload store
            if (ev.len > 0) tracked_update(ev.pgn, ev.data, ev.len);
            // dispatch to subscribers
            dispatch_event(&ev);
        }
    }
}

// Public API

esp_err_t vcan_receiver_init(void) {
    ensure_init_resources();
    if (!s_mutex || !s_event_q) return ESP_ERR_NO_MEM;
    // create worker task once
    if (!s_worker_task) {
        BaseType_t ok = xTaskCreate(vcan_worker_task, VCAN_WORKER_TASK_NAME, VCAN_WORKER_STACK, NULL, VCAN_WORKER_PRIO, &s_worker_task);
        if (ok != pdPASS) return ESP_ERR_NO_MEM;
    }
    // clear state
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        s_sub_count = 0;
        s_tracked_count = 0;
        s_last_heartbeat_seq = 0xFF;
        s_last_heartbeat_ms = 0;
        xSemaphoreGive(s_mutex);
    }
    return ESP_OK;
}

// Process a single incoming frame buffer (id LE + len + payload)
esp_err_t vcan_receiver_process_frame(const uint8_t *buf, size_t buf_len) {
    if (!buf || buf_len < 5) return ESP_ERR_INVALID_ARG;
    // parse id (LE)
    uint32_t id = read_u32_le(&buf[0]);
    uint8_t len = buf[4];
    if (len > 8) return ESP_ERR_INVALID_SIZE;
    if (buf_len < (size_t)(5 + len)) return ESP_ERR_INVALID_SIZE;
    uint16_t pgn = id_get_pgn(id);
    uint8_t src = id_get_src(id);

    // optional: filter by source
    if (src != ADDR_ECU_DAQ) {
        // ignore other sources by default
        return ESP_OK;
    }

    // push event to queue (non-blocking attempt first)
    vcan_event_t ev;
    ev.pgn = pgn;
    ev.src = src;
    ev.len = len;
    if (len) memcpy(ev.data, &buf[5], len);
    else memset(ev.data, 0, sizeof(ev.data));

    if (!s_event_q) return ESP_ERR_INVALID_STATE;
    BaseType_t ok = xQueueSendToBack(s_event_q, &ev, 0);
    if (ok != pdTRUE) {
        // queue full: try a short block; if still fails, drop and log
        ok = xQueueSendToBack(s_event_q, &ev, pdMS_TO_TICKS(10));
        if (ok != pdTRUE) {
            ESP_LOGW(TAG, "Event queue full, dropping PGN 0x%04X", pgn);
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t vcan_receiver_register_pgn(uint16_t pgn, vcan_pgn_callback_t cb, void *user_ctx) {
    if (!cb) return ESP_ERR_INVALID_ARG;
    ensure_init_resources();
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
    // check duplicates
    for (size_t i = 0; i < s_sub_count; ++i) {
        if (s_subs[i].pgn == pgn && s_subs[i].cb == cb && s_subs[i].user_ctx == user_ctx) {
            xSemaphoreGive(s_mutex);
            return ESP_OK; // already registered
        }
    }
    if (s_sub_count >= VCAN_MAX_SUBS) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }
    s_subs[s_sub_count].pgn = pgn;
    s_subs[s_sub_count].cb = cb;
    s_subs[s_sub_count].user_ctx = user_ctx;
    s_sub_count++;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t vcan_receiver_unregister_pgn(uint16_t pgn, vcan_pgn_callback_t cb, void *user_ctx) {
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
    size_t i = 0;
    while (i < s_sub_count) {
        bool match = (s_subs[i].pgn == pgn) &&
                     ((cb == NULL) || (s_subs[i].cb == cb)) &&
                     ((user_ctx == NULL) || (s_subs[i].user_ctx == user_ctx));
        if (match) {
            // remove by swapping last
            s_subs[i] = s_subs[--s_sub_count];
        } else {
            ++i;
        }
    }
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t vcan_receiver_get_last_payload(uint16_t pgn, uint8_t *out_buf, uint8_t *out_len, uint32_t *last_seen_ms) {
    if (!out_buf || !out_len) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
    for (size_t i = 0; i < s_tracked_count; ++i) {
        if (s_tracked[i].pgn == pgn) {
            *out_len = s_tracked[i].len;
            if (s_tracked[i].len) memcpy(out_buf, s_tracked[i].data, s_tracked[i].len);
            else memset(out_buf, 0, 8);
            if (last_seen_ms) *last_seen_ms = s_tracked[i].last_seen_ms;
            xSemaphoreGive(s_mutex);
            return ESP_OK;
        }
    }
    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

bool vcan_receiver_is_stale(uint32_t timeout_ms) {
    uint32_t now = now_ms();
    if (s_last_heartbeat_ms == 0) return true;
    return (now - s_last_heartbeat_ms) > timeout_ms;
}

void vcan_receiver_reset(void) {
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) return;
    s_sub_count = 0;
    s_tracked_count = 0;
    s_last_heartbeat_seq = 0xFF;
    s_last_heartbeat_ms = 0;
    xSemaphoreGive(s_mutex);
}
