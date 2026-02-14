#include "GaugePacket.h"
#include "debug.h"          // your DB_PRINT / DB_PRINTLN macros
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ------------------------------------------------------------
// Global gauge state + mutex
// ------------------------------------------------------------
GaugePacket g_gauge_state{};
SemaphoreHandle_t g_gauge_mutex = nullptr;

// ------------------------------------------------------------
// Initialize global state
// ------------------------------------------------------------
void gauge_state_init()
{
    if (!g_gauge_mutex) {
        g_gauge_mutex = xSemaphoreCreateMutex();
    }

    memset(&g_gauge_state, 0, sizeof(GaugePacket));
}

// ------------------------------------------------------------
// Thread-safe write
// ------------------------------------------------------------
void gauge_state_set(const GaugePacket& in)
{
    if (xSemaphoreTake(g_gauge_mutex, portMAX_DELAY)) {
        ESP_LOGI("STATE", "SET speed=%d rpm=%d", in.speed, in.rpm);
        g_gauge_state = in;
        xSemaphoreGive(g_gauge_mutex);
    }
}

// ------------------------------------------------------------
// Thread-safe read
// ------------------------------------------------------------
void gauge_state_get(GaugePacket& out)
{
    if (xSemaphoreTake(g_gauge_mutex, portMAX_DELAY)) {
        out = g_gauge_state;
        // ESP_LOGI("STATE", "GET speed=%d rpm=%d", out.speed, out.rpm);
        xSemaphoreGive(g_gauge_mutex);
    }
}

// ------------------------------------------------------------
// Debug print helper
// ------------------------------------------------------------
void printGaugePacket(const GaugePacket& pkt)
{
    DB_PRINTLN("----- GaugePacket -----");

    DB_PRINTLN("Speed: %d", pkt.speed);
    DB_PRINTLN("RPM: %d", pkt.rpm);
    DB_PRINTLN("Gear: %d", pkt.gearPosition);

    DB_PRINTLN("IA Temp: %d", pkt.iaTemp);
    DB_PRINTLN("Oil Temp: %d", pkt.oilTemp);
    DB_PRINTLN("Coolant Temp: %d", pkt.coolantTemp);
    DB_PRINTLN("Trans Temp: %d", pkt.transTemp);
    DB_PRINTLN("Ambient Temp: %d", pkt.ambientTemp);
    DB_PRINTLN("EGT: %d", pkt.EGTemp);

    DB_PRINTLN("Oil Pressure: %d", pkt.oilPressure);
    DB_PRINTLN("Fuel Pressure: %d", pkt.fuelPressure);
    DB_PRINTLN("Boost Pressure: %d", pkt.boostPressure);

    DB_PRINTLN("Accel X: %d", pkt.accelerationX);
    DB_PRINTLN("Accel Y: %d", pkt.accelerationY);
    DB_PRINTLN("Accel Z: %d", pkt.accelerationZ);

    DB_PRINTLN("Digital Pins: %u", pkt.digitalPins);

    DB_PRINTLN("Cruise Active: %u", pkt.cruiseActive);
    DB_PRINTLN("Cruise Set: %u", pkt.cruiseSetValue);

    DB_PRINTLN("--- GNSS ---");

    DB_PRINTLN("Date: %04u-%02u-%02u",
               pkt.year, pkt.month, pkt.day);

    DB_PRINTLN("Time: %02u:%02u:%02u",
               pkt.hour, pkt.minute, pkt.second);

    DB_PRINTLN("Heading: %.2f deg",
               pkt.headingDeg / 100.0f);

    DB_PRINTLN("Compass: %s", pkt.compass8);

    DB_PRINTLN("------------------------");
}
