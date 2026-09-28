#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>  // for NAN, isnan

typedef struct __attribute__((packed)) {

  // -------------------------
  // Core vehicle sensor data
  // -------------------------
  int16_t speed;
  int16_t rpm;
  uint32_t odometerTenths;
  int16_t gearPosition;

  int16_t batteryLevel;
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
  int16_t headingDeg;  // scaled degrees (0–35999 = 0–359.99)
  char compass8[4];    // "N", "NE", "SW", "UNK", etc.
  int32_t gpsLat;
  int32_t gpsLon;                                 // degrees * 1e7
  uint32_t gpsSpeed;                              // mm/s
  uint16_t gpsAltitude;                           // meters
  uint8_t gpsFix;                                 // 0=no fix, 2=2D, 3=3D
  uint8_t gpsSatCount;                            // satellites used
} GaugePacket;


// ------------------------------------------------------------
// fillGaugePacket()
// ------------------------------------------------------------
inline void fillGaugePacket(
  GaugePacket &pkt,

  // Vehicle data
  int16_t speed,
  int16_t rpm,
  uint32_t odometerTenths,
  int16_t gearPosition,
  int16_t fuelLevel,
  int16_t batteryLevel,
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
  int8_t month = -1,
  int8_t day = -1,
  int8_t hour = -1,
  int8_t minute = -1,
  int8_t second = -1,
  float headingDeg = NAN,
  const char *compass8 = nullptr,

  // GPS position/speed
  int32_t gpsLat = 0,
  int32_t gpsLon = 0,              
  uint32_t gpsSpeed = 0,
  uint16_t gpsAltitude = 0,
  uint8_t gpsFix = 0,
  uint8_t gpsSatCount = 0
) {
  // Vehicle
  pkt.speed = speed;
  pkt.rpm = rpm;
  pkt.odometerTenths = odometerTenths;
  pkt.gearPosition = gearPosition;
  pkt.fuelLevel = fuelLevel;
  pkt.batteryLevel = batteryLevel;

  pkt.iaTemp = iaTemp;
  pkt.oilTemp = oilTemp;
  pkt.coolantTemp = coolantTemp;
  pkt.transTemp = transTemp;
  pkt.ambientTemp = ambientTemp;
  pkt.EGTemp = EGTemp;

  pkt.oilPressure = oilPressure;
  pkt.fuelPressure = fuelPressure;
  pkt.boostPressure = boostPressure;

  pkt.accelerationX = accelerationX;
  pkt.accelerationY = accelerationY;
  pkt.accelerationZ = accelerationZ;

  pkt.digitalPins = digitalPins;

  pkt.cruiseActive = cruiseActive;
  pkt.cruiseSetValue = cruiseSetValue;

  // GNSS — only overwrite if provided
  if (year >= 0) pkt.year = (uint16_t)year;
  if (month >= 0) pkt.month = (uint8_t)month;
  if (day >= 0) pkt.day = (uint8_t)day;

  if (hour >= 0) pkt.hour = (uint8_t)hour;
  if (minute >= 0) pkt.minute = (uint8_t)minute;
  if (second >= 0) pkt.second = (uint8_t)second;

  if (!isnan(headingDeg))
    pkt.headingDeg = (int16_t)(headingDeg * 100.0f);

  if (compass8 != nullptr) {
    strncpy(pkt.compass8, compass8, sizeof(pkt.compass8));
    pkt.compass8[sizeof(pkt.compass8) - 1] = '\0';
  }
  // Store GPS lat/lon (scaled)
pkt.gpsLat      = gpsLat;
pkt.gpsLon      = gpsLon;
pkt.gpsSpeed    = gpsSpeed;
pkt.gpsAltitude = gpsAltitude;
pkt.gpsFix      = gpsFix;
pkt.gpsSatCount = gpsSatCount;
}

// ------------------------------------------------------------
// printGaugePacket()
// ------------------------------------------------------------
#include <esp_log.h>

static const char* TAG_PKT = "GaugePacket";

static inline void printGaugePacket(const GaugePacket &pkt)
{
    ESP_LOGI(TAG_PKT, "----- GaugePacket -----");

    ESP_LOGI(TAG_PKT, "Speed: %d", pkt.speed);
    ESP_LOGI(TAG_PKT, "RPM: %d", pkt.rpm);
    ESP_LOGI(TAG_PKT, "ODO (tenths): %u", pkt.odometerTenths);
    ESP_LOGI(TAG_PKT, "Gear: %d", pkt.gearPosition);
    ESP_LOGI(TAG_PKT, "FuelLevel: %d", pkt.fuelLevel);
    ESP_LOGI(TAG_PKT, "BatteryLevel: %d", pkt.batteryLevel);

    ESP_LOGI(TAG_PKT, "IA Temp: %d", pkt.iaTemp);
    ESP_LOGI(TAG_PKT, "Oil Temp: %d", pkt.oilTemp);
    ESP_LOGI(TAG_PKT, "Coolant Temp: %d", pkt.coolantTemp);
    ESP_LOGI(TAG_PKT, "Trans Temp: %d", pkt.transTemp);
    ESP_LOGI(TAG_PKT, "Ambient Temp: %d", pkt.ambientTemp);
    ESP_LOGI(TAG_PKT, "EGT: %d", pkt.EGTemp);

    ESP_LOGI(TAG_PKT, "Oil Pressure: %d", pkt.oilPressure);
    ESP_LOGI(TAG_PKT, "Fuel Pressure: %d", pkt.fuelPressure);
    ESP_LOGI(TAG_PKT, "Boost Pressure: %d", pkt.boostPressure);

    ESP_LOGI(TAG_PKT, "Accel X: %d", pkt.accelerationX);
    ESP_LOGI(TAG_PKT, "Accel Y: %d", pkt.accelerationY);
    ESP_LOGI(TAG_PKT, "Accel Z: %d", pkt.accelerationZ);

    ESP_LOGI(TAG_PKT, "Digital Pins: 0x%04X", pkt.digitalPins);

    ESP_LOGI(TAG_PKT, "Cruise Active: %u", pkt.cruiseActive);
    ESP_LOGI(TAG_PKT, "Cruise Set: %u", pkt.cruiseSetValue);

    ESP_LOGI(TAG_PKT, "--- GNSS ---");
    ESP_LOGI(TAG_PKT, "Date: %04u-%02u-%02u", pkt.year, pkt.month, pkt.day);
    ESP_LOGI(TAG_PKT, "Time: %02u:%02u:%02u", pkt.hour, pkt.minute, pkt.second);

    ESP_LOGI(TAG_PKT, "Heading: %.2f deg", pkt.headingDeg / 100.0f);
    ESP_LOGI(TAG_PKT, "Compass: %s", pkt.compass8);

    ESP_LOGI(TAG_PKT, "------------------------");
}
