#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>   // for NAN, isnan

typedef struct __attribute__((packed)) {

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
  uint8_t  month;
  uint8_t  day;

  uint8_t  hour;
  uint8_t  minute;
  uint8_t  second;

  // -------------------------
  // GNSS heading + direction
  // -------------------------
  int16_t headingDeg;     // scaled degrees (0–35999 = 0–359.99)
  char    compass8[4];    // "N", "NE", "SW", "UNK", etc.

} GaugePacket;


// ------------------------------------------------------------
// fillGaugePacket()
// ------------------------------------------------------------
inline void fillGaugePacket(
    GaugePacket &pkt,

    // Vehicle data
    int16_t speed,
    int16_t rpm,
    int16_t gearPosition,
    int16_t fuelLevel,
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
    uint16_t cruiseSetValue = 0,

    // GNSS data (optional)
    int16_t year = -1,
    int8_t  month = -1,
    int8_t  day = -1,
    int8_t  hour = -1,
    int8_t  minute = -1,
    int8_t  second = -1,
    float   headingDeg = NAN,
    const char *compass8 = nullptr
) {
    // Vehicle
    pkt.speed          = speed;
    pkt.rpm            = rpm;
    pkt.gearPosition   = gearPosition;
    pkt.fuelLevel      = fuelLevel;

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

    // GNSS — only overwrite if provided
    if (year   >= 0) pkt.year   = (uint16_t)year;
    if (month  >= 0) pkt.month  = (uint8_t)month;
    if (day    >= 0) pkt.day    = (uint8_t)day;

    if (hour   >= 0) pkt.hour   = (uint8_t)hour;
    if (minute >= 0) pkt.minute = (uint8_t)minute;
    if (second >= 0) pkt.second = (uint8_t)second;

    if (!isnan(headingDeg))
        pkt.headingDeg = (int16_t)(headingDeg * 100.0f);

    if (compass8 != nullptr) {
        strncpy(pkt.compass8, compass8, sizeof(pkt.compass8));
        pkt.compass8[sizeof(pkt.compass8)-1] = '\0';
    }
}

// ------------------------------------------------------------
// printGaugePacket()
// ------------------------------------------------------------
static inline void printGaugePacket(const GaugePacket &pkt) {
    Serial.println("----- GaugePacket -----");

    Serial.print("Speed: ");          Serial.println(pkt.speed);
    Serial.print("RPM: ");            Serial.println(pkt.rpm);
    Serial.print("Gear: ");           Serial.println(pkt.gearPosition);
    Serial.print("FuelLevel: ");      Serial.println(pkt.fuelLevel);

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

    Serial.println("--- GNSS ---");

    Serial.printf("Date: %04u-%02u-%02u\n", pkt.year, pkt.month, pkt.day);
    Serial.printf("Time: %02u:%02u:%02u\n", pkt.hour, pkt.minute, pkt.second);

    Serial.print("Heading: ");
    Serial.println(pkt.headingDeg / 100.0f);

    Serial.print("Compass: ");
    Serial.println(pkt.compass8);

    Serial.println("------------------------");
}