#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Must match vehicle_protocol.h definitions
#define ADDR_ECU_DAQ      0xFA

// PGNs (must match transmitter)
#define PGN_ENGINE_CORE    0xFF01
#define PGN_PRESSURES      0xFF02
#define PGN_TEMPS          0xFF03
#define PGN_EXHAUST_DIG    0xFF04
#define PGN_IMU_DYNAMICS   0xFF05
#define PGN_CRUISE_ODO     0xFF06
#define PGN_GPS_POS        0xFF10
#define PGN_GPS_MOTION     0xFF11
#define PGN_GPS_STATUS     0xFF12
#define PGN_GNSS_TIME      0xFF13
#define PGN_HEARTBEAT      0xFF20

// Callback invoked when a PGN payload arrives.
// pgn: PGN number, src: source address, payload: pointer to payload bytes, len: payload length, user_ctx: user pointer
typedef void (*vcan_pgn_callback_t)(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *user_ctx);

// Initialize receiver (call once). Returns ESP_OK on success.
esp_err_t vcan_receiver_init(void);

// Process a single incoming frame buffer (call from esp_now RX callback).
// Wire format expected: id (4 bytes little-endian) | len (1 byte) | payload (len bytes).
esp_err_t vcan_receiver_process_frame(const uint8_t *buf, size_t buf_len);

// Register/unregister callbacks for a PGN. Multiple callbacks allowed.
// Returns ESP_OK on success.
esp_err_t vcan_receiver_register_pgn(uint16_t pgn, vcan_pgn_callback_t cb, void *user_ctx);
esp_err_t vcan_receiver_unregister_pgn(uint16_t pgn, vcan_pgn_callback_t cb, void *user_ctx);

// Get last payload for a PGN. out_buf must have space for 8 bytes.
// Returns ESP_OK and sets *out_len and *last_seen_ms if found, ESP_ERR_NOT_FOUND otherwise.
esp_err_t vcan_receiver_get_last_payload(uint16_t pgn, uint8_t *out_buf, uint8_t *out_len, uint32_t *last_seen_ms);

// Check whether the feed is stale (no heartbeat within timeout_ms).
bool vcan_receiver_is_stale(uint32_t timeout_ms);

// Reset receiver state (clears tracked PGNs and subscriptions)
void vcan_receiver_reset(void);

#ifdef __cplusplus
}
#endif
