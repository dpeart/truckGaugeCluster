#include "GaugePacket.h"
#include "debug.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "GaugePacket";

// ------------------------------------------------------------
// Global gauge state + mutex
// ------------------------------------------------------------
GaugePacket g_gauge_state;          // no C++ initializer
SemaphoreHandle_t g_gauge_mutex = NULL;

// ------------------------------------------------------------
// Initialize global state
// ------------------------------------------------------------
void gauge_state_init(void)
{
    if (g_gauge_mutex == NULL) {
        g_gauge_mutex = xSemaphoreCreateMutex();
    }

    memset(&g_gauge_state, 0, sizeof(GaugePacket));
}

// ------------------------------------------------------------
// Thread-safe write
// ------------------------------------------------------------
void gauge_state_set(const GaugePacket *in)
{
    if (xSemaphoreTake(g_gauge_mutex, portMAX_DELAY)) {

        // ESP_LOGI("STATE", "SET speed=%d rpm=%d", in->speed, in->rpm);

        memcpy(&g_gauge_state, in, sizeof(GaugePacket));

        xSemaphoreGive(g_gauge_mutex);
    }
}

// ------------------------------------------------------------
// Thread-safe read
// ------------------------------------------------------------
void gauge_state_get(GaugePacket *out)
{
    if (xSemaphoreTake(g_gauge_mutex, portMAX_DELAY)) {

        memcpy(out, &g_gauge_state, sizeof(GaugePacket));

        xSemaphoreGive(g_gauge_mutex);
    }
}

// ------------------------------------------------------------
// Debug print helper
// ------------------------------------------------------------
void printGaugePacket(const GaugePacket *pkt)
{
    ESP_LOGI(TAG, "----- GaugePacket -----");

    ESP_LOGI(TAG, "Speed: %d", pkt->speed);
    ESP_LOGI(TAG, "RPM: %d", pkt->rpm);
    ESP_LOGI(TAG, "Gear: %d", pkt->gearPosition);
    ESP_LOGI(TAG, "Fuel Level: %d", pkt->fuelLevel);

    ESP_LOGI(TAG, "IA Temp: %d", pkt->iaTemp);
    ESP_LOGI(TAG, "Oil Temp: %d", pkt->oilTemp);
    ESP_LOGI(TAG, "Coolant Temp: %d", pkt->coolantTemp);
    ESP_LOGI(TAG, "Trans Temp: %d", pkt->transTemp);
    ESP_LOGI(TAG, "Ambient Temp: %d", pkt->ambientTemp);
    ESP_LOGI(TAG, "EGT: %d", pkt->EGTemp);

    ESP_LOGI(TAG, "Oil Pressure: %d", pkt->oilPressure);
    ESP_LOGI(TAG, "Fuel Pressure: %d", pkt->fuelPressure);
    ESP_LOGI(TAG, "Boost Pressure: %d", pkt->boostPressure);

    ESP_LOGI(TAG, "Accel X: %d", pkt->accelerationX);
    ESP_LOGI(TAG, "Accel Y: %d", pkt->accelerationY);
    ESP_LOGI(TAG, "Accel Z: %d", pkt->accelerationZ);

    ESP_LOGI(TAG, "Digital Pins: %u", pkt->digitalPins);

    ESP_LOGI(TAG, "Cruise Active: %u", pkt->cruiseActive);
    ESP_LOGI(TAG, "Cruise Set: %u", pkt->cruiseSetValue);

    ESP_LOGI(TAG, "--- GNSS ---");

    ESP_LOGI(TAG, "Date: %04u-%02u-%02u",
             pkt->year, pkt->month, pkt->day);

    ESP_LOGI(TAG, "Time: %02u:%02u:%02u",
             pkt->hour, pkt->minute, pkt->second);

    ESP_LOGI(TAG, "Heading: %.2f deg",
             pkt->headingDeg / 100.0f);

    ESP_LOGI(TAG, "Compass: %s", pkt->compass8);

    ESP_LOGI(TAG, "------------------------");
}