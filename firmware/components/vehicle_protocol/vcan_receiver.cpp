#include "vcan_receiver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>
#include <vector>

static const char *TAG = "VCAN-RECEIVER";

static inline uint32_t get_now_ms() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

namespace vcan {

Receiver& Receiver::instance() {
    static Receiver instance;
    return instance;
}

esp_err_t Receiver::init() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) return ESP_OK;

    if (!event_queue_) {
        event_queue_ = xQueueCreate(VCAN_EVENT_QUEUE_LEN, sizeof(Event));
        if (!event_queue_) {
            ESP_LOGE(TAG, "Failed to create event queue");
            return ESP_ERR_NO_MEM;
        }
    }

    if (!worker_task_) {
        xTaskCreate(
            Receiver::workerTaskStub,
            "vcan_rx_worker",
            4096,
            this,
            5,
            &worker_task_
        );
    }

    subscriptions_.reserve(16);
    tracked_pgns_.reserve(16);
    last_heartbeat_seq_ = 0xFF;
    last_heartbeat_ms_ = 0;
    initialized_ = true;

    ESP_LOGI(TAG, "vcan::Receiver initialized");
    return ESP_OK;
}

void Receiver::workerTaskStub(void *arg) {
    static_cast<Receiver*>(arg)->workerTask();
}

void Receiver::workerTask() {
    Event ev;
    auto q = static_cast<QueueHandle_t>(event_queue_);

    while (true) {
        if (xQueueReceive(q, &ev, portMAX_DELAY) == pdTRUE) {
            updateTracked(ev.pgn, ev.data.data(), ev.len);

            std::vector<PgnCallback> cbs_to_invoke;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto &sub : subscriptions_) {
                    if (sub.pgn == ev.pgn || sub.pgn == 0xFFFF) {
                        cbs_to_invoke.push_back(sub.callback);
                    }
                }
            }

            for (const auto &cb : cbs_to_invoke) {
                cb(ev.pgn, ev.data.data(), ev.len);
            }
        }
    }
}

void Receiver::updateTracked(uint16_t pgn, const uint8_t *payload, uint8_t len) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &t : tracked_pgns_) {
        if (t.pgn == pgn) {
            t.len = len;
            if (payload && len > 0) {
                memcpy(t.data.data(), payload, len);
            }
            t.last_seen_ms = get_now_ms();
            return;
        }
    }

    TrackedPgn new_entry{};
    new_entry.pgn = pgn;
    new_entry.len = len;
    if (payload && len > 0) {
        memcpy(new_entry.data.data(), payload, len);
    }
    new_entry.last_seen_ms = get_now_ms();
    tracked_pgns_.push_back(new_entry);
}

esp_err_t Receiver::processFrame(const uint8_t *buf, size_t buf_len) {
    if (!buf || buf_len < 4) return ESP_ERR_INVALID_ARG;

    // Standard header: [PGN High, PGN Low, Src Addr, Priority]
    uint16_t pgn = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    uint8_t payload_len = static_cast<uint8_t>(buf_len - 4);

    return dispatch(pgn, &buf[4], payload_len);
}

esp_err_t Receiver::dispatch(uint32_t pgn, const uint8_t *payload, uint8_t payload_len) {
    if (!event_queue_) return ESP_ERR_INVALID_STATE;

    Event ev{};
    ev.pgn = static_cast<uint16_t>(pgn);
    ev.len = payload_len;
    if (payload && payload_len > 0) {
        memcpy(ev.data.data(), payload, payload_len);
    }

    auto q = static_cast<QueueHandle_t>(event_queue_);
    if (xQueueSend(q, &ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full, dropped PGN 0x%04X", ev.pgn);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t Receiver::registerCallback(uint16_t pgn, PgnCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.push_back({pgn, std::move(cb)});
    return ESP_OK;
}

esp_err_t Receiver::getLastPayload(uint16_t pgn, uint8_t *out_buf, uint8_t *out_len, uint32_t *last_seen_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto &t : tracked_pgns_) {
        if (t.pgn == pgn) {
            if (out_buf) memcpy(out_buf, t.data.data(), t.len);
            if (out_len) *out_len = t.len;
            if (last_seen_ms) *last_seen_ms = t.last_seen_ms;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

bool Receiver::isStale(uint32_t timeout_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (last_heartbeat_ms_ == 0) return true;
    return (get_now_ms() - last_heartbeat_ms_) > timeout_ms;
}

void Receiver::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.clear();
    tracked_pgns_.clear();
    last_heartbeat_seq_ = 0xFF;
    last_heartbeat_ms_ = 0;
}

} // namespace vcan