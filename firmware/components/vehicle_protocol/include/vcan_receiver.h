#ifndef VCAN_RECEIVER_H
#define VCAN_RECEIVER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <mutex>
#include <vector>
#include <functional>
#include <array>
#include <cstdint>

namespace vcan {

constexpr size_t VCAN_EVENT_QUEUE_LEN = 32;

using PgnCallback = std::function<void(uint16_t pgn, const uint8_t *data, uint8_t len)>;

struct Event {
    uint16_t pgn;
    uint8_t len;
    std::array<uint8_t, 256> data;
};

struct TrackedPgn {
    uint16_t pgn;
    uint8_t len;
    std::array<uint8_t, 256> data;
    uint32_t last_seen_ms;
};

struct Subscription {
    uint16_t pgn;
    PgnCallback callback;
};

class Receiver {
public:
    static Receiver& instance();

    esp_err_t init();
    esp_err_t processFrame(const uint8_t *buf, size_t buf_len);
    esp_err_t dispatch(uint32_t id, const uint8_t *payload, uint8_t payload_len);
    esp_err_t registerCallback(uint16_t pgn, PgnCallback cb);
    esp_err_t getLastPayload(uint16_t pgn, uint8_t *out_buf, uint8_t *out_len, uint32_t *last_seen_ms);
    bool isStale(uint32_t timeout_ms) const;
    void reset();

private:
    Receiver() = default;
    ~Receiver() = default;

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    static void workerTaskStub(void *arg);
    void workerTask();
    void updateTracked(uint16_t pgn, const uint8_t *payload, uint8_t len);

    mutable std::mutex mutex_;
    bool initialized_ = false;
    QueueHandle_t event_queue_ = nullptr;
    TaskHandle_t worker_task_ = nullptr;

    std::vector<Subscription> subscriptions_;
    std::vector<TrackedPgn> tracked_pgns_;

    uint8_t last_heartbeat_seq_ = 0xFF;
    uint32_t last_heartbeat_ms_ = 0;
};

} // namespace vcan

#endif // VCAN_RECEIVER_H