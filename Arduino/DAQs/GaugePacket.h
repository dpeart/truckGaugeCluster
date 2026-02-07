#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
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
} GaugePacket;
