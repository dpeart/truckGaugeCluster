#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Build Configuration Flags
// -----------------------------------------------------------------------------
// Set to 1 during development to enable synthetic debug data.
// Set to 0 for production to completely exclude debug code & static footprint.
#ifndef CONFIG_DAQ_ENABLE_DEBUG_SIM
#define CONFIG_DAQ_ENABLE_DEBUG_SIM 0
#endif

// -----------------------------------------------------------------------------
// Circuit Breaker / Health Configuration
// -----------------------------------------------------------------------------
#define MAX_CONSECUTIVE_ERRORS 5

typedef struct {
    uint32_t total_errors;
    uint8_t consecutive_errors;
    bool circuit_breaker_tripped;
    uint32_t last_error_time_ms;
} dev_health_t;

// -----------------------------------------------------------------------------
// DAQ Structures
// -----------------------------------------------------------------------------
typedef struct {
    int16_t rpm;
    int16_t speed;
    int16_t gearPosition;
    int16_t coolantTemp;
    int16_t oilPressure;
    int16_t fuelPressure;
    int16_t boostPressure;
    int16_t batteryLevel;
    int16_t oilTemp;
    int16_t transTemp;
    int16_t ambientTemp;
    int16_t iaTemp;
    int32_t egTemp;
    int16_t fuelLevel;
    uint16_t digitalPins;
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
    uint16_t cruiseActive;
    uint16_t cruiseSetValue;
    uint32_t odometerTenths;
    int32_t lat;
    int32_t lon;
    uint32_t gpsSpeed;
    uint16_t gpsAltitude;
    int16_t headingDeg;
    uint8_t gpsFix;
    uint8_t gpsSatCount;
    char compass4[4];
    uint16_t gnssYear;
    uint8_t gnssMonth;
    uint8_t gnssDay;
    uint8_t gnssHour;
    uint8_t gnssMinute;
    uint8_t gnssSecond;
} daq_cache_t;

// -----------------------------------------------------------------------------
// API Declarations
// -----------------------------------------------------------------------------

/**
 * @brief Initialize DAQ cache resources and mutexes.
 */
esp_err_t daq_cache_init(void);

/**
 * @brief Poll hardware off-mutex, perform health checks, and commit snapshot.
 */
esp_err_t daq_cache_update(void);

/**
 * @brief Thread-safe retrieval of current DAQ cache snapshot.
 */
esp_err_t daq_cache_get(daq_cache_t *out_cache);

/**
 * @brief Reset latched circuit breaker state for hardware recovery.
 */
void daq_cache_reset_circuit_breakers(void);

#ifdef __cplusplus
}
#endif