#include "Globals.h"
#include "daq_cache.h"
#include "vcan_protocol.h"
#include "vcan_sender.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "DAQ_CACHE";

// Core global storage and mutex
static daq_cache_t g_daq_cache = {};
static SemaphoreHandle_t g_cache_mutex = NULL;

// Device tracking states (Latched Circuit Breakers)
static dev_health_t g_univin_health = {};
static dev_health_t g_rtd_health = {};
static dev_health_t g_digin_health = {};

// Helper: timestamp in milliseconds
static inline uint32_t get_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// -----------------------------------------------------------------------------
// Health / Circuit Breaker Logic
// -----------------------------------------------------------------------------
static bool check_and_update_health(dev_health_t *health, esp_err_t status, const char *dev_name)
{
    if (health->circuit_breaker_tripped)
    {
        return false; // Skip execution while latched
    }

    if (status == ESP_OK)
    {
        health->consecutive_errors = 0;
        return true;
    }

    // Failure branch
    health->total_errors++;
    health->consecutive_errors++;
    health->last_error_time_ms = get_now_ms();

    ESP_LOGW(TAG, "%s I2C read failed (%s) [%d/%d]",
             dev_name, esp_err_to_name(status),
             health->consecutive_errors, MAX_CONSECUTIVE_ERRORS);

    if (health->consecutive_errors >= MAX_CONSECUTIVE_ERRORS)
    {
        health->circuit_breaker_tripped = true;
        ESP_LOGE(TAG, "Circuit breaker TRIPPED for %s. Disabling device.", dev_name);
        return false;
    }

    return true;
}

void daq_cache_reset_circuit_breakers(void)
{
    if (xSemaphoreTake(g_cache_mutex, portMAX_DELAY) == pdTRUE)
    {
        memset(&g_univin_health, 0, sizeof(dev_health_t));
        memset(&g_rtd_health, 0, sizeof(dev_health_t));
        memset(&g_digin_health, 0, sizeof(dev_health_t));
        xSemaphoreGive(g_cache_mutex);
        ESP_LOGI(TAG, "All circuit breakers reset.");
    }
}

// -----------------------------------------------------------------------------
// Optional Debug Simulation Code (Excluded entirely when DEBUG_SIMULATION_MODE == 0)
// -----------------------------------------------------------------------------
static void inject_debug_simulation(daq_cache_t *cache)
{
    static uint16_t sim = 0;
    sim++;

    /*
     * SENSOR RANGE & STEP FORMULA:
     * -----------------------------------------------------------------------------
     * To step through a range [MIN .. MAX] by a specific STEP size and guarantee
     * hitting both MIN and MAX without overflow or skipped bounds:
     *
     *   STEPS = ((MAX - MIN) / STEP) + 1
     *   VALUE = MIN + ((sim % STEPS) * STEP)
     *   WIRE  = VALUE * INT_SCALING
     *
     * Example (EGT: 100 to 1500 in steps of 25):
     *   STEPS = ((1500 - 100) / 25) + 1 = 57
     *   cache->egTemp = (100 + ((sim % 57) * 25)) * INT_SCALING;
     * -----------------------------------------------------------------------------
     */

    if (DEBUG_SIMULATION_MODE)
    {
        // --- Engine Core ---
        cache->rpm = (0 + (sim % 5000));       // 0 -> 5,000 RPM    (Wire: 0 -> 500,000, Requires 32-bit field)
        cache->speed = (0 + (sim % 141));      // 0 -> 140 MPH/KPH   (Wire: 0 -> 14,000)
        cache->gearPosition = 1 + ((sim / 100) % 6); // Gears 1 -> 6       (Discrete state, unscaled)
        cache->odometerTenths += (sim % 2);          // Odometer           (Discrete counter, unscaled)

        // --- Pressures ---
        cache->oilPressure = (0 + (sim % 91)) * INT_SCALING;   // 0 -> 90 PSI        (Wire: 0 -> 9,000)
        cache->fuelPressure = (20 + (sim % 51)) * INT_SCALING; // 20 -> 70 PSI       (Wire: 2,000 -> 7,000)
        cache->boostPressure = (0 + (sim % 61)) * INT_SCALING; // 0 -> 60 PSI        (Wire: 0 -> 6,000)
        cache->batteryLevel = (100 + (sim % 48)) * 10;         // 10.0V -> 14.7V     (Wire: 1,200 -> 1,470)

        // --- Temperatures (°F or °C) ---
        cache->coolantTemp = (150 + (sim % 101)) * INT_SCALING; // 150 -> 250 °F/°C   (Wire: 15,000 -> 25,000)
        cache->oilTemp = (150 + (sim % 151)) * INT_SCALING;     // 150 -> 300 °F/°C   (Wire: 15,000 -> 30,000)
        cache->transTemp = (80 + (sim % 181)) * INT_SCALING;    // 80 -> 260 °F/°C    (Wire: 8,000 -> 26,000)
        cache->iaTemp = (50 + (sim % 151)) * INT_SCALING;       // 50 -> 200 °F/°C    (Wire: 5,000 -> 20,000 Intake Air)
        cache->ambientTemp = (5 + (sim % 100)) * INT_SCALING;   // 5 -> 105 °F/°C     (Wire: 6,500 -> 8,400)
        cache->egTemp = (100 + (sim % 29) * 50) * INT_SCALING;  // 100 -> 1,500 °F/°C (Wire: 10,000 -> 150,000, Requires 32-bit field)

        // --- Fuel Level ---
        cache->fuelLevel = ((sim / 5) % 101) * INT_SCALING; // 0 -> 100%
        cache->digitalPins = (sim % 256);                   // Bitmask sweep across digital inputs

        // --- IMU / Acceleration ---
        cache->accelX = -500 + (sim % 1000); // G-force sweep (-0.5g -> +0.5g)
        cache->accelY = -500 + ((sim * 2) % 1000);
        cache->accelZ = 980 + (sim % 40); // ~1.0g gravity static base

        // --- GPS & Motion ---
        cache->lat = 37774929 + (sim % 1000); // Simulated coordinate drift
        cache->lon = -122419416 + (sim % 1000);
        cache->gpsSpeed = (sim / 10) % 120;
        cache->gpsAltitude = 100 + (sim % 50);
        cache->headingDeg = (sim * 5) % 36000; // 0.00 -> 359.99 degrees
        cache->gpsFix = 1;                     // Valid GPS Fix
        cache->gpsSatCount = 8 + (sim % 5);    // 8 -> 12 satellites

        // Compass cardinal direction cycling
        static const char *dirs[] = {"N  ", "NE ", "E  ", "SE ", "S  ", "SW ", "W  ", "NW "};
        const char *current_dir = dirs[(sim / 50) % 8];
        memcpy(cache->compass4, current_dir, 4);

        // --- GNSS Time Sweep ---
        cache->gnssYear = 2026;
        cache->gnssMonth = 10;
        cache->gnssDay = 3;
        cache->gnssHour = (sim / 3600) % 24;
        cache->gnssMinute = (sim / 60) % 60;
        cache->gnssSecond = sim % 60;
    }
}

// -----------------------------------------------------------------------------
// Core DAQ Operations
// -----------------------------------------------------------------------------
esp_err_t daq_cache_init(void)
{
    if (!g_cache_mutex)
    {
        g_cache_mutex = xSemaphoreCreateMutex();
        if (!g_cache_mutex)
        {
            ESP_LOGE(TAG, "Failed to create DAQ mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    memset(&g_daq_cache, 0, sizeof(daq_cache_t));
    daq_cache_reset_circuit_breakers();

    ESP_LOGI(TAG, "DAQ Cache initialized (Debug Sim: %s)",
             DEBUG_SIMULATION_MODE ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

esp_err_t daq_cache_update(void)
{
    // 1. Create stack workspace for off-mutex sampling
    daq_cache_t working_cache;

    // Pre-populate working cache with existing global state as base
    if (xSemaphoreTake(g_cache_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        working_cache = g_daq_cache;
        xSemaphoreGive(g_cache_mutex);
    }
    else
    {
        memset(&working_cache, 0, sizeof(daq_cache_t));
    }

    // 2. Sample hardware off-mutex directly into working_cache

    // --- UNIVIN / Analog Card Reading ---
    if (!g_univin_health.circuit_breaker_tripped)
    {
        // Example: esp_err_t err = read_univin_card(&raw_univin);
        esp_err_t err = ESP_OK; // Replace with actual driver call
        if (check_and_update_health(&g_univin_health, err, "UNIVIN_CARD"))
        {
            // Directly assign mapped signals
            // working_cache.oilPressure = raw_univin.chan[0];
            // working_cache.fuelPressure = raw_univin.chan[1];
        }
    }

    // --- RTD / Temp Card Reading ---
    if (!g_rtd_health.circuit_breaker_tripped)
    {
        // Example: esp_err_t err = read_rtd_card(&raw_rtd);
        esp_err_t err = ESP_OK; // Replace with actual driver call
        if (check_and_update_health(&g_rtd_health, err, "RTD_CARD"))
        {
            // working_cache.coolantTemp = raw_rtd.temp[0];
            // working_cache.oilTemp     = raw_rtd.temp[1];
        }
    }

    // --- Digital Inputs Card Reading ---
    if (!g_digin_health.circuit_breaker_tripped)
    {
        // Example: esp_err_t err = read_digin_card(&raw_pins);
        esp_err_t err = ESP_OK; // Replace with actual driver call
        if (check_and_update_health(&g_digin_health, err, "DIGIN_CARD"))
        {
            // working_cache.digitalPins = raw_pins;
        }
    }

    // 3. Inject Debug Mock Data if compiled in
    if (DEBUG_SIMULATION_MODE)
    {
        inject_debug_simulation(&working_cache);
    }

    // 4. Commit to global cache under fast mutex lock (< 1 microsecond hold time)
    if (xSemaphoreTake(g_cache_mutex, portMAX_DELAY) == pdTRUE)
    {
        memcpy(&g_daq_cache, &working_cache, sizeof(daq_cache_t));
        xSemaphoreGive(g_cache_mutex);
    }

    // 5. Broadcast frame updates via vCAN
    vcan_send_engine_core(working_cache.rpm, working_cache.speed,
                          working_cache.gearPosition, working_cache.coolantTemp);

    vcan_send_pressures(working_cache.oilPressure, working_cache.fuelPressure,
                        working_cache.boostPressure, working_cache.batteryLevel);

    vcan_send_temps(working_cache.oilTemp, working_cache.transTemp,
                    working_cache.ambientTemp, working_cache.iaTemp);

    return ESP_OK;
}

esp_err_t daq_cache_get(daq_cache_t *out_cache)
{
    if (!out_cache)
        return ESP_ERR_INVALID_ARG;
    if (!g_cache_mutex)
        return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(g_cache_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        memcpy(out_cache, &g_daq_cache, sizeof(daq_cache_t));
        xSemaphoreGive(g_cache_mutex);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}