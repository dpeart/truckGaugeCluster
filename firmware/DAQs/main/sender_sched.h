#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sender_sched_init(void);
void sender_sched_register(uint32_t pgn, uint8_t priority, uint32_t min_interval_ms);
void sender_sched_tick(void);

#ifdef __cplusplus
}
#endif
