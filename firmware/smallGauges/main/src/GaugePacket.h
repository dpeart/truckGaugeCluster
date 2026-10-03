#ifndef GAUGE_PACKET_H
#define GAUGE_PACKET_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    // --- Indicator booleans ---
    bool ind_water_in_fuel;
    bool ind_low_washer;
    bool ind_low_fuel;
    bool ind_low_battery;
    bool ind_engine_temp;

    // --- Info screen ---
    int16_t ambientTemp;     // raw ambient temp
    char    compass8[8];     // heading string
    uint8_t hour;
    uint8_t minute;

    // --- Optional raw digital bitmap (for debugging)
    uint16_t digitalPins;

} GaugePacket;

#endif
