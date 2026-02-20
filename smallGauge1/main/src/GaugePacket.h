#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once
#include <stdint.h>
#include <string.h>

    typedef struct __attribute__((packed))
    {

        // -------------------------
        // Core vehicle sensor data
        // -------------------------
        int16_t speed;
        int16_t rpm;
        int16_t gearPosition;
        int16_t fuelLevel;

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

        // -------------------------
        // GNSS date/time (local)
        // -------------------------
        uint16_t year;
        uint8_t month;
        uint8_t day;

        uint8_t hour;
        uint8_t minute;
        uint8_t second;

        // -------------------------
        // GNSS heading + direction
        // -------------------------
        int16_t headingDeg; // scaled degrees (0–35999 = 0–359.99)
        char compass8[4];   // "N", "NE", "SW", "UNK", etc.

    } GaugePacket;

    extern GaugePacket g_gauge_state;

    void gauge_state_init(void);
    void gauge_state_set(const GaugePacket *in);
    void gauge_state_get(GaugePacket *out);
    void printGaugePacket(const GaugePacket *pkt);

#ifdef __cplusplus
}
#endif