#pragma once
#include <cstdint>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


// ------------------------------------------------------------
// GaugePacket (pure C++ struct)
// ------------------------------------------------------------
struct  __attribute__((packed)) GaugePacket {

    // Core vehicle sensor data
    int16_t speed;
    int16_t rpm;
    int16_t gearPosition;

    int16_t iaTemp;
    int16_t oilTemp;
    int16_t coolantTemp;
    int16_t transTemp;
    int16_t ambientTemp;
    int16_t EGTemp;

    int16_t oilPressure;
    int16_t fuelPressure;
    int16_t boostPressure;

    int16_t accelerationX;
    int16_t accelerationY;
    int16_t accelerationZ;

    uint16_t digitalPins;

    uint16_t cruiseActive;
    uint16_t cruiseSetValue;

    // GNSS date/time
    uint16_t year;
    uint8_t  month;
    uint8_t  day;

    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;

    // GNSS heading + direction
    int16_t headingDeg;     // scaled degrees (0–35999 = 0–359.99)
    char    compass8[4];    // "N", "NE", "SW", etc.
};

// ------------------------------------------------------------
// Global Gauge State API (C++)
// ------------------------------------------------------------

// Global shared state (defined in GaugePacket.cpp)
extern GaugePacket g_gauge_state;
extern SemaphoreHandle_t g_gauge_mutex;

// Initialize mutex + zero state
void gauge_state_init();

// Thread-safe write
void gauge_state_set(const GaugePacket& in);

// Thread-safe read
void gauge_state_get(GaugePacket& out);

// Debug helper (implemented in .cpp)
void printGaugePacket(const GaugePacket& pkt);
