#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct __attribute__((packed))
  {

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
    int16_t headingDeg;   // scaled degrees (0–35999 = 0–359.99)
    char compass8[4];     // "N", "NE", "SW", "UNK", etc.
    int32_t gpsLat;       // degrees * 1e7
    int32_t gpsLon;       // degrees * 1e7
    uint32_t gpsSpeed;    // mm/s
    uint16_t gpsAltitude; // meters
    uint8_t gpsFix;       // 0=no fix, 2=2D, 3=3D
    uint8_t gpsSatCount;  // satellites used

  } GaugePacket;

  extern GaugePacket g_gauge_state;

  // Global queue handle for GaugePacket
  extern QueueHandle_t g_gauge_queue;

  // State API
  void gauge_state_init(void);
  void gauge_state_set(const GaugePacket *in);
  void gauge_state_get(GaugePacket *out);
  bool gauge_state_is_stale(uint32_t timeout_ms);
  void printGaugePacket(const GaugePacket *pkt);

  // Queue API
  void gauge_queue_init(void);
  bool gauge_queue_push_from_isr(const GaugePacket *pkt, BaseType_t *pxHigherPriorityTaskWoken);
  bool gauge_queue_push(const GaugePacket *pkt);
  bool gauge_queue_pop(GaugePacket *out, TickType_t timeout);

#ifdef __cplusplus
}
#endif
