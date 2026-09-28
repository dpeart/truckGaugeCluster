#ifndef DEBUG_H
#define DEBUG_H

#include "esp_log.h"

/* Control whether debugging macros are active at compile time */
#undef DB_ACTIVE
#ifdef DEBUG
#define DB_ACTIVE 1
#else
#define DB_ACTIVE 0
#endif /* DEBUG */

/* Default tag used by DB_PRINT macros if caller doesn't provide one.
   You can override by defining DB_TAG before including debug.h in a file. */
#ifndef DB_TAG
#define DB_TAG "APP"
#endif

/* DB_PRINT macros map to ESP_LOGD when active; otherwise compile to nothing. */
#if DB_ACTIVE
  #define DB_PRINT(fmt, ...)  do { ESP_LOGD(DB_TAG, fmt, ##__VA_ARGS__); } while (0)
  #define DB_PRINTF(fmt, ...) do { ESP_LOGD(DB_TAG, fmt, ##__VA_ARGS__); } while (0)
  #define DB_PRINTLN(fmt, ...) do { ESP_LOGD(DB_TAG, fmt, ##__VA_ARGS__); } while (0)
#else
  #define DB_PRINT(...)    do { (void)0; } while (0)
  #define DB_PRINTF(...)   do { (void)0; } while (0)
  #define DB_PRINTLN(...)  do { (void)0; } while (0)
#endif

#endif // DEBUG_H
