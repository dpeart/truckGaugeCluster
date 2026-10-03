#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "esp_log.h"
#include <esp_timer.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "Globals.h"
#include "CruiseControl.h"
#include "interruptHandlers.h"
#include "vcan_protocol.h"
#include "vcan_sender.h"
#include "sender_sched.h"
#include "daq_cache.h"

// -------------------- LOGGING --------------------
static const char *TAG = "GaugeCluster";

bool DEBUG_SIMULATION_MODE = true;

// -------------------- GLOBAL STATE --------------------
uint64_t previousMillis = 0;

// Global I2C bus handle
i2c_master_bus_handle_t bus_handle = NULL;

// -------------------- Small helpers --------------------
static inline int16_t clamp_int32_to_int16(int32_t v) {
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

static inline uint16_t clamp_uint32_to_uint16(uint32_t v) {
    if (v > UINT16_MAX) return UINT16_MAX;
    return (uint16_t)v;
}

// -------------------- I2C init --------------------
static void i2c_master_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO,
        .scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,     // 0 = driver selects default priority
        .trans_queue_depth = 0, // 0 = default queue depth (not using async)
        .flags = {
            .enable_internal_pullup = true,
            .allow_pd = 0, // 0 = keep power domain alive in light sleep
        },
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
}

// -------------------- ESP-NOW INIT --------------------
void initEspNow(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peerInfo = {};
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ESP-NOW peer");
    } else {
        ESP_LOGI(TAG, "ESP-NOW peer added successfully");
    }

    if (vcan_init_transmitter() != ESP_OK) {
        ESP_LOGW(TAG, "vcan_init_transmitter failed");
    }
}

void scanI2CBus(i2c_master_bus_handle_t bus)
{
    ESP_LOGI("I2C_SCAN", "Scanning I2C bus...");
    int devices_found = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_err_t ret = i2c_master_probe(bus, addr, 50);
        if (ret == ESP_OK) {
            ESP_LOGI("I2C_SCAN", "Found device at address: 0x%02X", addr);
            devices_found++;
        }
    }

    if (devices_found == 0) {
        ESP_LOGW("I2C_SCAN", "No I2C devices found!");
    }
}

#include "esp_log.h"

// Place this at the top of app_main() before driver initializations
void configure_log_levels(void)
{
    // 1. Global default: Only show Warnings and Errors across the system
    esp_log_level_set("*", ESP_LOG_WARN);

    // 2. Main Application Tags
    esp_log_level_set("GaugeCluster",    ESP_LOG_INFO);  // main.cpp app state
    esp_log_level_set("DAQ_CACHE",       ESP_LOG_VERBOSE);  // Cache updates & circuit breakers

    // 3. System Schedulers & vCAN Communication
    esp_log_level_set("sender_sched",    ESP_LOG_VERBOSE);  // Set to ESP_LOG_INFO for tx debugging

    // 4. Hardware Component Drivers
    esp_log_level_set("mcp960x",         ESP_LOG_NONE);  // Thermocouple driver logging

    // 5. Test/Scratchpad Modules (Kept quiet during standard runs)
    esp_log_level_set("GNSS_TEST",       ESP_LOG_NONE);
    esp_log_level_set("ADC_TEST",        ESP_LOG_NONE);
    esp_log_level_set("RTD_TEST",        ESP_LOG_NONE);
    esp_log_level_set("MAIN_TEST",       ESP_LOG_NONE);
    esp_log_level_set("MERGED_SYSTEM",   ESP_LOG_NONE);

    esp_log_level_set("VCAN_RX",         ESP_LOG_VERBOSE);
    esp_log_level_set("VCAN-SENDER",     ESP_LOG_VERBOSE);
}

// -------------------- MAIN --------------------
extern "C" void app_main(void)
{
  // Apply granular tag log levels
    configure_log_levels();

    i2c_master_init();
    scanI2CBus(bus_handle);
    initEspNow();
    initInterruptHandlers();

    // Initialize DAQ Cache System
    if (daq_cache_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DAQ Cache!");
    }

    // Initialize sender scheduler and register PGNs
    sender_sched_init();

    sender_sched_register(PGN_HEARTBEAT, CAN_PRIORITY_HIGH, 1000);  // 1s
    sender_sched_register(PGN_ENGINE_CORE, CAN_PRIORITY_HIGH, 200); // 5Hz
    sender_sched_register(PGN_PRESSURES, CAN_PRIORITY_HIGH, 250);   // 4Hz
    sender_sched_register(PGN_TEMPS, CAN_PRIORITY_MED, 500);        // 2Hz
    sender_sched_register(PGN_EXHAUST_DIG, CAN_PRIORITY_MED, 250);  // 4Hz
    sender_sched_register(PGN_CRUISE_ODO, CAN_PRIORITY_LOW, 1000);  // 1s
    sender_sched_register(PGN_GPS_POS, CAN_PRIORITY_MED, 1000);     // 1s
    sender_sched_register(PGN_GPS_MOTION, CAN_PRIORITY_MED, 500);   // 2Hz
    sender_sched_register(PGN_GPS_STATUS, CAN_PRIORITY_LOW, 1000);  // 1s
    sender_sched_register(PGN_GNSS_TIME, CAN_PRIORITY_HIGH, 1000);  // 1s

    previousMillis = esp_timer_get_time();

    while (true)
    {
        uint64_t now = esp_timer_get_time();

        // 1. Poll DAQ boards off-mutex and commit snapshot
        daq_cache_update();

        // 2. Transmission tick every 16ms (~60Hz)
        if (now - previousMillis >= 16000ULL)
        {
            previousMillis = now;

            // Fetch thread-safe DAQ snapshot under fast mutex lock
            daq_cache_t daq;
            if (daq_cache_get(&daq) != ESP_OK) {
                vTaskDelay(1);
                continue; 
            }

            // Extract & Clamp Snapshot Data safely
            int16_t send_rpm      = clamp_int32_to_int16(daq.rpm);
            int16_t send_speed    = clamp_int32_to_int16(daq.speed);
            int16_t send_gear     = clamp_int32_to_int16(daq.gearPosition);
            int16_t send_coolant  = clamp_int32_to_int16(daq.coolantTemp);

            int16_t send_oil      = clamp_int32_to_int16(daq.oilPressure);
            int16_t send_fuel     = clamp_int32_to_int16(daq.fuelPressure);
            int16_t send_boost    = clamp_int32_to_int16(daq.boostPressure);
            int16_t send_batt     = clamp_int32_to_int16(daq.batteryLevel);

            int16_t send_oiltemp  = clamp_int32_to_int16(daq.oilTemp);
            int16_t send_transt   = clamp_int32_to_int16(daq.transTemp);
            int16_t send_ambient  = clamp_int32_to_int16(daq.ambientTemp);
            int16_t send_iat      = clamp_int32_to_int16(daq.iaTemp);

            int32_t send_egt      = daq.egTemp;
            int16_t send_fuellvl  = clamp_int32_to_int16(daq.fuelLevel);
            uint16_t send_dig     = daq.digitalPins;

            int16_t ax            = clamp_int32_to_int16(daq.accelX);
            int16_t ay            = clamp_int32_to_int16(daq.accelY);
            int16_t az            = clamp_int32_to_int16(daq.accelZ);

            uint16_t cruise_act   = daq.cruiseActive;
            uint16_t cruise_set   = daq.cruiseSetValue;
            uint32_t odo_tenths   = daq.odometerTenths;

            // Transmit scaled vCAN Frames
            vcan_send_engine_core(send_rpm, send_speed, send_gear, send_coolant);
            vcan_send_pressures(send_oil, send_fuel, send_boost, send_batt);
            vcan_send_temps(send_oiltemp, send_transt, send_ambient, send_iat);
            vcan_send_exhaust_dig(send_egt, send_fuellvl, send_dig);
            vcan_send_imu_dynamics(ax, ay, az);
            vcan_send_cruise_odo(cruise_act, cruise_set, odo_tenths);
            vcan_send_gps_position(daq.lat, daq.lon);
            vcan_send_gps_motion(daq.gpsSpeed, daq.gpsAltitude, daq.headingDeg);
            vcan_send_gps_status(daq.gpsFix, daq.gpsSatCount, daq.compass4);

            // Execute priority scheduler
            sender_sched_tick();
        }

        vTaskDelay(1);
    }
}