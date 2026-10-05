#include "Globals.h"
#include "daq_cache.h"
#include "vcan_protocol.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Card Driver Headers
#include "SM_16UNIVIN.h"
#include "SM_RTD.h"
#include "SM_16DIGIN.h"
#include "mcp960x.h"
#include "DFRobot_GNSS.h"
#include "AuberinsSensors.h"

#include <stdint.h>
#include <string.h>
#include <cmath>
#include <algorithm>

static const char *TAG = "DAQ_CACHE";

// Core global storage and mutex
static daq_cache_t g_daq_cache = {};
static SemaphoreHandle_t g_cache_mutex = NULL;

// Device tracking states (Latched Circuit Breakers)
static dev_health_t g_univin_health = {};
static dev_health_t g_rtd_health = {};
static dev_health_t g_digin_health = {};
static dev_health_t g_mcp9601_health = {};
static dev_health_t g_gnss_health = {};

// Card Driver Instances
static SM_16_UNIVIN *g_univin_card = nullptr;
static SM_RTD *g_rtd_card = nullptr;
static SM_16DIGIN *g_digin_card = nullptr;
static DFRobot_GNSS_I2C *g_gnss_card = nullptr;
static mcp960x_t g_mcp9601_dev = {};
static bool g_mcp9601_ready = false;

// Helper: timestamp in milliseconds
static inline uint32_t get_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/**
 * Calculates current ambient atmospheric pressure in PSI based on altitude.
 *
 * @param altitudeMeters Raw altitude from DAQ cache (uint16_t).
 * @param gpsFix Fix status flag from DAQ cache (0 = no fix, >0 = valid fix).
 * @return Local atmospheric pressure in PSIA.
 */
float getAmbientBaroPsi(uint16_t altitudeMeters, uint8_t gpsFix)
{
    float altitudeFt = GOSHEN_ALTITUDE_FT;

    // Check if GPS fix is active and altitude is within reasonable atmospheric limits (<= 9000 meters)
    if (gpsFix > 0 && altitudeMeters < 9000)
    {
        altitudeFt = static_cast<float>(altitudeMeters) * 3.28084f;
    }

    // Standard International Barometric Formula
    float baroPsi = 14.69595f * std::pow(1.0f - (2.25577e-5f * altitudeFt), 5.25588f);
    return baroPsi;
}

// -----------------------------------------------------------------------------
// Health / Circuit Breaker Logic
// -----------------------------------------------------------------------------
static bool check_and_update_health(dev_health_t *health, esp_err_t status, const char *dev_name)
{
    if (health->circuit_breaker_tripped)
    {
        return false;
    }

    if (status == ESP_OK)
    {
        health->consecutive_errors = 0;
        return true;
    }

    health->total_errors++;
    health->consecutive_errors++;
    health->last_error_time_ms = get_now_ms();

    ESP_LOGW(TAG, "%s read failed (%s) [%d/%d]",
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
        memset(&g_mcp9601_health, 0, sizeof(dev_health_t));
        memset(&g_gnss_health, 0, sizeof(dev_health_t));
        xSemaphoreGive(g_cache_mutex);
        ESP_LOGI(TAG, "All circuit breakers reset.");
    }
}

// -----------------------------------------------------------------------------
// Hardware Driver Read Helpers
// -----------------------------------------------------------------------------
static esp_err_t read_univin_card(daq_cache_t *cache)
{
    if (!g_univin_card || !g_univin_card->isAlive())
    {
        return ESP_ERR_NOT_FOUND;
    }

    int batt_mv = g_univin_card->readAnalogMv(1);
    if (batt_mv < 0)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }
    float batt_volts = batt_mv / 1000.0f;
    cache->batteryLevel = static_cast<int16_t>(batt_volts * INT_SCALING);

    int oil_mv = g_univin_card->readAnalogMv(ADC_OIL_PRESSURE);
    int fuel_mv = g_univin_card->readAnalogMv(ADC_FUEL_PRESSURE);
    int boost_mv = g_univin_card->readAnalogMv(ADC_BOOST_PRESSURE);

    if (oil_mv < 0 || fuel_mv < 0 || boost_mv < 0)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }

    float ambientBaroPsi = getAmbientBaroPsi(cache->gpsAltitude, cache->gpsFix);

    cache->oilPressure = static_cast<int16_t>(calculatePressure5BAR(static_cast<float>(oil_mv), ambientBaroPsi));
    cache->fuelPressure = static_cast<int16_t>(calculatePressure5BAR(static_cast<float>(fuel_mv), ambientBaroPsi));
    cache->boostPressure = static_cast<int16_t>(calculatePressure5BAR(static_cast<float>(boost_mv), ambientBaroPsi));
    return ESP_OK;
}

static esp_err_t read_rtd_card(daq_cache_t *cache)
{
    if (!g_rtd_card || !g_rtd_card->isAlive())
    {
        return ESP_ERR_NOT_FOUND;
    }

    float c_coolant = g_rtd_card->readTemp(1);
    float c_oil = g_rtd_card->readTemp(2);
    float c_trans = g_rtd_card->readTemp(3);
    float c_ambient = g_rtd_card->readTemp(4);
    float c_ia = g_rtd_card->readTemp(5);

    if (c_coolant <= -900.0f || c_oil <= -900.0f || c_trans <= -900.0f ||
        c_ambient <= -900.0f || c_ia <= -900.0f)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }

    cache->coolantTemp = static_cast<int16_t>(((c_coolant * 1.8f) + 32.0f) * INT_SCALING);
    cache->oilTemp = static_cast<int16_t>(((c_oil * 1.8f) + 32.0f) * INT_SCALING);
    cache->transTemp = static_cast<int16_t>(((c_trans * 1.8f) + 32.0f) * INT_SCALING);
    cache->ambientTemp = static_cast<int16_t>(((c_ambient * 1.8f) + 32.0f) * INT_SCALING);
    cache->iaTemp = static_cast<int16_t>(((c_ia * 1.8f) + 32.0f) * INT_SCALING);

    return ESP_OK;
}

static esp_err_t read_digin_card(daq_cache_t *cache)
{
    if (!g_digin_card || !g_digin_card->isAlive())
    {
        return ESP_ERR_NOT_FOUND;
    }

    int pins = g_digin_card->readInputs();
    if (pins < 0)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }

    cache->digitalPins = static_cast<uint16_t>(pins);
    return ESP_OK;
}

static esp_err_t read_mcp9601_card(daq_cache_t *cache)
{
    if (!g_mcp9601_ready)
    {
        return ESP_ERR_NOT_FOUND;
    }

    float c_egt = 0.0f;
    esp_err_t err = mcp960x_get_thermocouple_temp(&g_mcp9601_dev, &c_egt);
    if (err != ESP_OK || std::isnan(c_egt) || c_egt < -100.0f)
    {
        return (err != ESP_OK) ? err : ESP_ERR_INVALID_RESPONSE;
    }

    float f_egt = (c_egt * 1.8f) + 32.0f;
    cache->egTemp = static_cast<int32_t>(f_egt * INT_SCALING);

    return ESP_OK;
}

static esp_err_t read_gnss_card(daq_cache_t *cache)
{
    if (!g_gnss_card)
    {
        return ESP_ERR_NOT_FOUND;
    }

    // Time & Date
    sTim_t date = g_gnss_card->getDate();
    sTim_t utc = g_gnss_card->getUTC();

    cache->gnssYear = static_cast<uint16_t>(date.year);
    cache->gnssMonth = static_cast<uint8_t>(date.month);
    cache->gnssDay = static_cast<uint8_t>(date.date);
    cache->gnssHour = static_cast<uint8_t>(utc.hour);
    cache->gnssMinute = static_cast<uint8_t>(utc.minute);
    cache->gnssSecond = static_cast<uint8_t>(utc.second);

    // Geographic Coordinates (Decimal Degrees * 1e6)
    sLonLat_t lat = g_gnss_card->getLat();
    sLonLat_t lon = g_gnss_card->getLon();

    cache->lat = static_cast<int32_t>(lat.latitudeDegree * 1e6f);
    cache->lon = static_cast<int32_t>(lon.lonitudeDegree * 1e6f);

    // Speed (Knots to MPH -> uint32_t)
    float speedKnots = g_gnss_card->getSog();
    cache->gpsSpeed = static_cast<uint32_t>(speedKnots * 1.15078f);

    // Altitude (Meters -> uint16_t)
    float altMeters = g_gnss_card->getAlt();
    cache->gpsAltitude = static_cast<uint16_t>(altMeters > 0.0f ? altMeters : 0.0f);

    // Fix & Satellites
    uint8_t sats = g_gnss_card->getNumSatUsed();
    cache->gpsSatCount = sats;
    cache->gpsFix = (sats >= 4) ? 1 : 0;

    return ESP_OK;
}

static inline int32_t scale_sine_float(float rad, float min_val, float max_val)
{
    float norm = (std::sin(rad) + 1.0f) * 0.5f;
    return static_cast<int32_t>(min_val + (norm * (max_val - min_val)));
}

static void inject_debug_simulation(daq_cache_t *cache)
{
    if (!DEBUG_SIMULATION_MODE || !cache)
    {
        ESP_LOGW(TAG, "Simulation skipped: mode disabled or null cache pointer");
        return;
    }

    static uint8_t step = 0;
    step++;

    float rad_main = (static_cast<float>(step) / 255.0f) * 2.0f * 3.14159265f;
    float rad_off1 = std::fmod(rad_main + 1.57079632f, 2.0f * 3.14159265f);
    float rad_off2 = std::fmod(rad_main + 3.14159265f, 2.0f * 3.14159265f);

    cache->rpm = static_cast<uint16_t>(scale_sine_float(rad_main, 800.0f, 5000.0f));
    cache->speed = static_cast<uint16_t>(scale_sine_float(rad_main, 0.0f, 140.0f));
    cache->gearPosition = static_cast<uint8_t>(1 + (cache->speed / 25));
    cache->odometerTenths += (cache->speed > 10) ? 1 : 0;

    cache->oilPressure = scale_sine_float(rad_main, 10.0f, 90.0f) * INT_SCALING;
    cache->fuelPressure = scale_sine_float(rad_off1, 20.0f, 70.0f) * INT_SCALING;
    cache->boostPressure = scale_sine_float(rad_main, 0.0f, 30.0f) * INT_SCALING;
    cache->batteryLevel = scale_sine_float(rad_main, 10.0f, 14.7f) * INT_SCALING;

    cache->coolantTemp = scale_sine_float(rad_main, 150.0f, 250.0f) * INT_SCALING;
    cache->oilTemp = scale_sine_float(rad_off1, 150.0f, 300.0f) * INT_SCALING;
    cache->transTemp = scale_sine_float(rad_off2, 80.0f, 250.0f) * INT_SCALING;
    cache->iaTemp = scale_sine_float(rad_main, 50.0f, 200.0f) * INT_SCALING;
    cache->ambientTemp = scale_sine_float(rad_off1, 5.0f, 105.0f) * INT_SCALING;
    cache->egTemp = scale_sine_float(rad_main, 200.0f, 1400.0f) * INT_SCALING;

    cache->fuelLevel = scale_sine_float(rad_off2, 0.0f, 100.0f) * INT_SCALING;

    uint8_t active_pin = (step / 8) % 15;
    cache->digitalPins = (1U << active_pin);

    cache->accelX = static_cast<int16_t>(scale_sine_float(rad_off1, -500.0f, 500.0f));
    cache->accelY = static_cast<int16_t>(scale_sine_float(rad_off2, -500.0f, 500.0f));
    cache->accelZ = static_cast<int16_t>(scale_sine_float(rad_main, 960.0f, 1020.0f));

    cache->lat = 37774929 + scale_sine_float(rad_main, 0.0f, 500.0f);
    cache->lon = -122419416 + scale_sine_float(rad_main, 0.0f, 500.0f);
    cache->gpsSpeed = cache->speed;
    cache->gpsAltitude = static_cast<uint16_t>(scale_sine_float(rad_off1, 100.0f, 250.0f));
    cache->headingDeg = static_cast<int16_t>(scale_sine_float(rad_main, 0.0f, 35999.0f));
    cache->gpsFix = 1;
    cache->gpsSatCount = static_cast<uint8_t>(scale_sine_float(rad_off2, 8.0f, 12.0f));

    static const char *dirs[8] = {"N  ", "NE ", "E  ", "SE ", "S  ", "SW ", "W  ", "NW "};
    int32_t heading_positive = (cache->headingDeg < 0) ? 0 : cache->headingDeg;
    uint32_t dir_idx = (((heading_positive / 100) + 22) / 45) % 8;
    memcpy(cache->compass4, dirs[dir_idx], 4);

    cache->gnssYear = 2026;
    cache->gnssMonth = 10;
    cache->gnssDay = 4;
    cache->gnssHour = (step / 10) % 24;
    cache->gnssMinute = (step * 2) % 60;
    cache->gnssSecond = (step * 5) % 60;
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

    if (!g_univin_card)
        g_univin_card = new SM_16_UNIVIN(I2C_MASTER_NUM, 0);
    if (!g_rtd_card)
        g_rtd_card = new SM_RTD(I2C_MASTER_NUM, 0);
    if (!g_digin_card)
        g_digin_card = new SM_16DIGIN(I2C_MASTER_NUM, 0);
    if (!g_gnss_card)
        g_gnss_card = new DFRobot_GNSS_I2C(I2C_MASTER_NUM, I2C_GNSS_ADR);

    if (g_univin_card->begin())
    {
        ESP_LOGI(TAG, "16UNIVIN card connected.");
    }
    else
    {
        ESP_LOGW(TAG, "16UNIVIN card failed to begin.");
    }

    if (g_rtd_card->begin())
    {
        ESP_LOGI(TAG, "SM_RTD card connected.");
    }
    else
    {
        ESP_LOGW(TAG, "SM_RTD card failed to begin.");
    }

    if (g_digin_card->begin())
    {
        ESP_LOGI(TAG, "16DIGIN card connected.");
    }
    else
    {
        ESP_LOGW(TAG, "16DIGIN card failed to begin.");
    }

    if (g_gnss_card->begin())
    {
        g_gnss_card->enablePower();
        g_gnss_card->setGnss(eGPS_BeiDou);
        g_gnss_card->setRgbOn();
        ESP_LOGI(TAG, "DFRobot GNSS module connected.");
    }
    else
    {
        ESP_LOGW(TAG, "DFRobot GNSS module failed to begin.");
    }

    esp_err_t mcp_err = mcp960x_init_desc(&g_mcp9601_dev, MCP960X_ADDR_DEFAULT, I2C_MASTER_NUM);
    if (mcp_err == ESP_OK)
    {
        mcp_err = mcp960x_init(&g_mcp9601_dev);
    }

    if (mcp_err == ESP_OK)
    {
        mcp960x_set_sensor_config(&g_mcp9601_dev, MCP960X_TYPE_K, 0);
        g_mcp9601_ready = true;
        ESP_LOGI(TAG, "MCP9601 EGT sensor connected.");
    }
    else
    {
        g_mcp9601_ready = false;
        ESP_LOGW(TAG, "MCP9601 EGT sensor failed to begin (%s).", esp_err_to_name(mcp_err));
    }

    memset(&g_daq_cache, 0, sizeof(daq_cache_t));
    daq_cache_reset_circuit_breakers();

    ESP_LOGI(TAG, "DAQ Cache initialized (Debug Sim: %s)",
             DEBUG_SIMULATION_MODE ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

esp_err_t daq_cache_update(void)
{
    daq_cache_t working_cache;

    if (xSemaphoreTake(g_cache_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        working_cache = g_daq_cache;
        xSemaphoreGive(g_cache_mutex);
    }
    else
    {
        memset(&working_cache, 0, sizeof(daq_cache_t));
    }

    if (!g_univin_health.circuit_breaker_tripped)
    {
        esp_err_t err = read_univin_card(&working_cache);
        check_and_update_health(&g_univin_health, err, "UNIVIN_CARD");
    }

    if (!g_rtd_health.circuit_breaker_tripped)
    {
        esp_err_t err = read_rtd_card(&working_cache);
        check_and_update_health(&g_rtd_health, err, "RTD_CARD");
    }

    if (!g_digin_health.circuit_breaker_tripped)
    {
        esp_err_t err = read_digin_card(&working_cache);
        check_and_update_health(&g_digin_health, err, "DIGIN_CARD");
    }

    if (!g_mcp9601_health.circuit_breaker_tripped)
    {
        esp_err_t err = read_mcp9601_card(&working_cache);
        check_and_update_health(&g_mcp9601_health, err, "MCP9601_CARD");
    }

    if (!g_gnss_health.circuit_breaker_tripped)
    {
        esp_err_t err = read_gnss_card(&working_cache);
        check_and_update_health(&g_gnss_health, err, "GNSS_CARD");
    }

    if (DEBUG_SIMULATION_MODE)
    {
        inject_debug_simulation(&working_cache);
    }

    static uint32_t last_log_ms = 0;
    uint32_t now = get_now_ms();

    if (now - last_log_ms >= 1000) //[cite: 3]
    {
        ESP_LOGI(TAG, "=== REAL DAQ BROADCAST FRAME ==="); //[cite: 3]
        ESP_LOGI(TAG, "  RPM: %u | Speed: %u mph | Gear: %u",
                 working_cache.rpm, working_cache.speed, working_cache.gearPosition); //[cite: 3]

        ESP_LOGI(TAG, "  Batt: %d (%.2fV) | Fuel: %d (%.1f%%) | DigPins: 0x%04X",
                 working_cache.batteryLevel,
                 (float)working_cache.batteryLevel / INT_SCALING,
                 working_cache.fuelLevel,
                 (float)working_cache.fuelLevel / INT_SCALING,
                 working_cache.digitalPins); //[cite: 3]

        ESP_LOGI(TAG, "  Temps (°F): Coolant=%.1f, Trans=%.1f, IAT=%.1f, EGT=%.1f",
                 (float)working_cache.coolantTemp / INT_SCALING,
                 (float)working_cache.transTemp / INT_SCALING,
                 (float)working_cache.iaTemp / INT_SCALING,
                 (float)working_cache.egTemp / INT_SCALING); //[cite: 3]

        ESP_LOGI(TAG, "  Pressures (PSI): Oil=%.1f, Fuel=%.1f, Boost=%.1f",
                 (float)working_cache.oilPressure / INT_SCALING,
                 (float)working_cache.fuelPressure / INT_SCALING,
                 (float)working_cache.boostPressure / INT_SCALING); //[cite: 3]

        // FIXED: gpsHasLock -> gpsFix
        ESP_LOGI(TAG, "  GNSS UTC: %04u-%02u-%02u %02u:%02u:%02u | Sats: %u | Lock: %s",
                 working_cache.gnssYear, working_cache.gnssMonth, working_cache.gnssDay,
                 working_cache.gnssHour, working_cache.gnssMinute, working_cache.gnssSecond,
                 working_cache.gpsSatCount, (working_cache.gpsFix > 0) ? "YES" : "NO");

        // FIXED: gpsAltitudeMeters -> gpsAltitude
        ESP_LOGI(TAG, "  GNSS Pos: Lat %.6f, Lon %.6f | Alt: %um | Speed: %u mph",
                 (float)working_cache.lat / 1e6f, (float)working_cache.lon / 1e6f,
                 working_cache.gpsAltitude, working_cache.gpsSpeed);

        last_log_ms = now; //[cite: 3]
    }

    if (xSemaphoreTake(g_cache_mutex, portMAX_DELAY) == pdTRUE)
    {
        memcpy(&g_daq_cache, &working_cache, sizeof(daq_cache_t));
        xSemaphoreGive(g_cache_mutex);
    }

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