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

inline void fillGaugePacket(
    GaugePacket &pkt,
    int16_t speed,
    int16_t rpm,
    int16_t gearPosition,
    int16_t iaTemp,
    int16_t oilTemp,
    int16_t coolantTemp,
    int16_t transTemp,
    int16_t ambientTemp,
    int16_t EGTemp,
    int16_t oilPressure,
    int16_t fuelPressure,
    int16_t boostPressure,
    int16_t accelerationX,
    int16_t accelerationY,
    int16_t accelerationZ,
    uint16_t digitalPins,
    uint16_t cruiseActive = 0,
    uint16_t cruiseSetValue = 0
) {
    pkt.speed          = speed;
    pkt.rpm            = rpm;
    pkt.gearPosition   = gearPosition;

    pkt.iaTemp         = iaTemp;
    pkt.oilTemp        = oilTemp;
    pkt.coolantTemp    = coolantTemp;
    pkt.transTemp      = transTemp;
    pkt.ambientTemp    = ambientTemp;
    pkt.EGTemp         = EGTemp;

    pkt.oilPressure    = oilPressure;
    pkt.fuelPressure   = fuelPressure;
    pkt.boostPressure  = boostPressure;

    pkt.accelerationX  = accelerationX;
    pkt.accelerationY  = accelerationY;
    pkt.accelerationZ  = accelerationZ;

    pkt.digitalPins    = digitalPins;

    pkt.cruiseActive   = cruiseActive;
    pkt.cruiseSetValue = cruiseSetValue;
}

static inline void printGaugePacket(const GaugePacket &pkt) {
    Serial.println("----- GaugePacket -----");

    Serial.print("Speed: ");          Serial.println(pkt.speed);
    Serial.print("RPM: ");            Serial.println(pkt.rpm);
    Serial.print("Gear: ");           Serial.println(pkt.gearPosition);

    Serial.print("IA Temp: ");        Serial.println(pkt.iaTemp);
    Serial.print("Oil Temp: ");       Serial.println(pkt.oilTemp);
    Serial.print("Coolant Temp: ");   Serial.println(pkt.coolantTemp);
    Serial.print("Trans Temp: ");     Serial.println(pkt.transTemp);
    Serial.print("Ambient Temp: ");   Serial.println(pkt.ambientTemp);
    Serial.print("EGT: ");            Serial.println(pkt.EGTemp);

    Serial.print("Oil Pressure: ");   Serial.println(pkt.oilPressure);
    Serial.print("Fuel Pressure: ");  Serial.println(pkt.fuelPressure);
    Serial.print("Boost Pressure: "); Serial.println(pkt.boostPressure);

    Serial.print("Accel X: ");        Serial.println(pkt.accelerationX);
    Serial.print("Accel Y: ");        Serial.println(pkt.accelerationY);
    Serial.print("Accel Z: ");        Serial.println(pkt.accelerationZ);

    Serial.print("Digital Pins: 0b"); Serial.println(pkt.digitalPins, BIN);

    Serial.print("Cruise Active: ");  Serial.println(pkt.cruiseActive);
    Serial.print("Cruise Set: ");     Serial.println(pkt.cruiseSetValue);

    Serial.println("------------------------");
}
