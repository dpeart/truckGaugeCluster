#ifndef ESPNOW_RECEIVER_H
#define ESPNOW_RECEIVER_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t espnow_receiver_init(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif // ESPNOW_RECEIVER_H