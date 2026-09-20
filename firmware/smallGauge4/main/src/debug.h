#pragma once
#include "esp_log.h"

// Enable/disable debug
#ifdef DEBUG
#define DB_ACTIVE 1
#else
#define DB_ACTIVE 0
#endif

#ifndef DB_TAG
#define DB_TAG "DEBUG"
#endif

// Print without newline
#define DB_PRINT(fmt, ...) \
    do { if (DB_ACTIVE) ESP_LOGI(DB_TAG, fmt, ##__VA_ARGS__); } while (0)

// Print with newline (same as ESP_LOGI already adds newline)
#define DB_PRINTLN(fmt, ...) \
    do { if (DB_ACTIVE) ESP_LOGI(DB_TAG, fmt, ##__VA_ARGS__); } while (0)

// printf-style
#define DB_PRINTF(fmt, ...) \
    do { if (DB_ACTIVE) ESP_LOGI(DB_TAG, fmt, ##__VA_ARGS__); } while (0)
