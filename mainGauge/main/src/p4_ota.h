#pragma once
#include <cstdint>

typedef enum {
    OTA_EVENT_CHUNK,
    OTA_EVENT_FINALIZE
} ota_event_type_t;

typedef struct {
    ota_event_type_t type;
    uint32_t len;
    uint8_t data[256];
} ota_event_t;

void p4_ota_begin();
void p4_ota_write(const uint8_t *data, uint32_t len);
void p4_ota_init(void);
void p4_ota_task(void *arg);
void p4_ota_queue_chunk(const uint8_t *data, uint32_t len);
void p4_ota_finalize();
void p4_ota_finalize_event(void);
