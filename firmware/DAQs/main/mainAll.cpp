#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"

// Drivers and Helpers
#include "esp_i2c_helpers.h"
#include "Globals.h"
#include "mcp960x.h"
#include "DFRobot_GNSS.h"
#include "SM_16DIGIN.h"
#include "SM_16UNIVIN.h"
#include "SM_RTD.h"

static const char *TAG = "MERGED_SYSTEM";

// Mappings from Globals.h for ADC logging output
struct AdcMap {
    uint8_t channel;
    const char *name;
};

static const AdcMap adcInputs[] = {
    {ADC_GEAR,           "GEAR_POSITION"},
    {ADC_TPS,            "THROTTLE_POSITION"},
    {ADC_FUEL_LEVEL,     "FUEL_LEVEL"},
    {ADC_BOOST_PRESSURE, "BOOST_PRESSURE"},
    {ADC_OIL_PRESSURE,   "OIL_PRESSURE"},
    {ADC_FUEL_PRESSURE,  "FUEL_PRESSURE"}
};

// Device Instances
static mcp960x_t mcpDev;
static DFRobot_GNSS_I2C gnss(I2C_MASTER_NUM, I2C_GNSS_ADR);
static SM_16DIGIN digCard(I2C_MASTER_NUM, 0);
static SM_16_UNIVIN adcCard(I2C_MASTER_NUM, 0);
static SM_RTD rtdCard(I2C_MASTER_NUM, 0);

// Hardware status flags
static bool mcp_detected = false;
static bool gnss_detected = false;
static bool dig_detected = false;
static bool adc_detected = false;
static bool rtd_detected = false;

// GNSS raw stream callback matching original GNSS harness
void gnssDataCallback(char *data, uint8_t len)
{
    char buffer[256] = {0};
    if (len < sizeof(buffer))
    {
        memcpy(buffer, data, len);
        printf("%s", buffer);
    }
}

/**
 * @brief Scans the specified I2C bus for connected devices and prints their addresses.
 */
void scan_i2c_bus(i2c_port_t port)
{
    ESP_LOGI(TAG, "Starting I2C bus scan...");
    uint8_t devices_found = 0;

    for (uint8_t addr = 1; addr < 127; addr++)
    {
        uint8_t dummy = 0;
        esp_err_t res = esp_i2c_read_reg(port, addr, 0x00, &dummy, 1);
        if (res == ESP_OK)
        {
            ESP_LOGI(TAG, "Found device at 7-bit address: 0x%02X (8-bit read: 0x%02X)",
                     addr, (addr << 1) | 1);
            devices_found++;
        }
    }

    if (devices_found == 0)
    {
        ESP_LOGW(TAG, "No I2C devices found. Check SDA/SCL pins, power, and pull-up resistors.");
    }
    else
    {
        ESP_LOGI(TAG, "Scan complete. Found %d device(s).", devices_found);
    }
}

/**
 * @brief Task: Polls MCP960X Thermocouple Sensor
 */
void mcp960x_task(void *pvParameters) {
    while (1) {
        if (mcp_detected) {
            float hot_temp = 0.0f;
            float cold_temp = 0.0f;
            float delta_temp = 0.0f;
            bool temp_ready = false;
            mcp960x_status_t dev_status = MCP960X_OK;

            // Check readiness and circuit status
            if (mcp960x_get_status(&mcpDev, &temp_ready, NULL, &dev_status, NULL, NULL, NULL, NULL) == ESP_OK) {
                if (dev_status == MCP960X_OPEN_CIRCUIT) {
                    ESP_LOGW(TAG, "[MCP960X] Warning: Thermocouple open circuit detected!");
                } else if (dev_status == MCP960X_SHORT_CIRCUIT) {
                    ESP_LOGW(TAG, "[MCP960X] Warning: Thermocouple short circuit detected!");
                }
            }

            // Read temperatures
            esp_err_t res_hot = mcp960x_get_thermocouple_temp(&mcpDev, &hot_temp);
            esp_err_t res_cold = mcp960x_get_ambient_temp(&mcpDev, &cold_temp);
            esp_err_t res_delta = mcp960x_get_delta_temp(&mcpDev, &delta_temp);

            if (res_hot == ESP_OK && res_cold == ESP_OK && res_delta == ESP_OK) {
                ESP_LOGI(TAG, "[MCP960X] Hot Junction: %6.2f °C | Cold Junction: %6.2f °C | Delta: %6.2f °C", 
                         hot_temp, cold_temp, delta_temp);
            } else {
                ESP_LOGE(TAG, "[MCP960X] Failed to read temperature data from MCP960X");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Task: Polls GNSS Location & Time
 */
void gnss_task(void *pvParameters) {
    while (1) {
        if (gnss_detected) {
            sTim_t utc = gnss.getUTC();
            sTim_t date = gnss.getDate();
            sLonLat_t lat = gnss.getLat();
            sLonLat_t lon = gnss.getLon();
            uint8_t sats = gnss.getNumSatUsed();

            ESP_LOGI(TAG, "-----------------------------------");
            ESP_LOGI(TAG, "[GNSS] Date: %04d-%02d-%02d | UTC: %02d:%02d:%02d",
                     date.year, date.month, date.date,
                     utc.hour, utc.minute, utc.second);
            ESP_LOGI(TAG, "[GNSS] Lat: %.6f ° %c | Lon: %.6f ° %c",
                     lat.latitudeDegree, lat.latDirection,
                     lon.lonitudeDegree, lon.lonDirection);
            ESP_LOGI(TAG, "[GNSS] Satellites in Use: %d", sats);
            ESP_LOGI(TAG, "[GNSS] Altitude: %.2f m | Speed: %.2f knots",
                     gnss.getAlt(), gnss.getSog());

            ESP_LOGI(TAG, "[GNSS] Raw Data Stream:");
            gnss.getAllGnss();
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief Task: Polls Sequent 16DIGIN Inputs
 */
void digin_task(void *pvParameters) {
    while (1) {
        if (dig_detected) {
            int inputsBitmap = digCard.readInputs();
            if (inputsBitmap >= 0) {
                printf("[16DIGIN] Inputs Bitmap: %d (0x%04X)\n", inputsBitmap, inputsBitmap);
            } else {
                printf("[16DIGIN] Read Error!\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief Task: Polls Sequent 16UNIVIN Analog Inputs
 */
void adc_task(void *pvParameters) {
    size_t adc_idx = 0;
    const size_t total_adc = sizeof(adcInputs) / sizeof(adcInputs[0]);

    while (1) {
        if (adc_detected) {
            uint8_t adc_ch = adcInputs[adc_idx].channel;
            const char *adc_name = adcInputs[adc_idx].name;

            // Read channel voltage in millivolts (0 .. 10000 mV)
            int mv = adcCard.readAnalogMv(adc_ch);

            if (mv >= 0) {
                ESP_LOGI(TAG, "[ADC CH %02d - %s] Voltage: %d mV (%.2f V)", 
                         adc_ch, 
                         adc_name, 
                         mv, 
                         mv / 1000.0f);
            } else {
                ESP_LOGE(TAG, "[ADC CH %02d - %s] Read Error!", adc_ch, adc_name);
            }

            adc_idx++;
            if (adc_idx >= total_adc) {
                adc_idx = 0;
                ESP_LOGI(TAG, "[16UNIVIN] ---------------------------------------------");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/**
 * @brief Task: Polls Sequent SM_RTD Channels
 */
void rtd_task(void *pvParameters) {
    uint8_t channel = 1;

    while (1) {
        if (rtd_detected) {
            // Read channel temperature in degrees Celsius
            float tempC = rtdCard.readTemp(channel);
            // Read raw channel resistance in Ohms
            float resOhms = rtdCard.readRes(channel);

            if (tempC > -900.0f) {
                ESP_LOGI(TAG, "[RTD CH %d] Temp: %.2f °C | Resistance: %.2f Ω", 
                         channel, tempC, resOhms);
            } else {
                ESP_LOGE(TAG, "[RTD CH %d] Read Error (Code: %.0f)!", channel, tempC);
            }

            channel++;
            if (channel > RTD_CHANNEL_NR_MAX) {
                channel = 1;
                ESP_LOGI(TAG, "[SM_RTD] ---------------------------------------------");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Initializing Shared I2C Bus...");

    // 1. Initialize Shared I2C Bus Driver
    esp_err_t err = esp_i2c_init(I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus driver: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "I2C Bus Initialized Successfully.");

    // 2. Scan the bus for connected hardware
    scan_i2c_bus(I2C_MASTER_NUM);

    // 3. Initialize MCP960X Thermocouple
    mcp960x_init_desc(&mcpDev, MCP960X_ADDR_DEFAULT, I2C_MASTER_NUM);
    esp_err_t mcp_err = mcp960x_init(&mcpDev);
    if (mcp_err == ESP_OK) {
        mcp_detected = true;
        ESP_LOGI(TAG, "MCP960X detected! Device ID: 0x%02X, Revision: 0x%02X", mcpDev.id, mcpDev.revision);
        mcp960x_set_sensor_config(&mcpDev, MCP960X_TYPE_K, MCP960X_FILTER_OFF);
        mcp960x_set_device_config(&mcpDev, MCP960X_MODE_NORMAL, MCP960X_SAMPLES_1, MCP960X_ADC_RES_18, MCP960X_TC_RES_0_0625);
    } else {
        ESP_LOGE(TAG, "Failed to connect to MCP960X at address 0x%02X (Error: %s)", mcpDev.addr, esp_err_to_name(mcp_err));
    }

    // 4. Initialize DFRobot GNSS
    ESP_LOGI(TAG, "Checking GNSS module connection...");
    if (gnss.begin()) {
        gnss_detected = true;
        ESP_LOGI(TAG, "GNSS module detected!");
        gnss.enablePower();
        gnss.setGnss(eGPS_BeiDou);
        gnss.setRgbOn();
        gnss.setCallback(gnssDataCallback);
    } else {
        ESP_LOGW(TAG, "GNSS module not detected.");
    }

    // 5. Initialize Sequent 16DIGIN Card
    if (digCard.begin()) {
        dig_detected = true;
        printf("16DIGIN Card detected!\n");
    } else {
        printf("16DIGIN Card not responding.\n");
    }

    // 6. Initialize Sequent 16UNIVIN Card
    if (adcCard.begin()) {
        adc_detected = true;
        ESP_LOGI(TAG, "16UNIVIN Card detected at address 0x%02X", ADC_CARD);
    } else {
        ESP_LOGW(TAG, "16UNIVIN Card not detected at address 0x%02X!", ADC_CARD);
    }

    // 7. Initialize Sequent SM_RTD Card
    if (rtdCard.begin()) {
        rtd_detected = true;
        ESP_LOGI(TAG, "SM_RTD Card detected at address 0x40");
    } else {
        ESP_LOGW(TAG, "SM_RTD Card not detected at address 0x40!");
    }

    // 8. Launch Dedicated Monitoring Tasks
    xTaskCreate(mcp960x_task, "mcp960x_task", 3072, NULL, 5, NULL);
    xTaskCreate(gnss_task,    "gnss_task",    4096, NULL, 5, NULL);
    xTaskCreate(digin_task,   "digin_task",   3072, NULL, 5, NULL);
    xTaskCreate(adc_task,     "adc_task",     3072, NULL, 5, NULL);
    xTaskCreate(rtd_task,     "rtd_task",     3072, NULL, 5, NULL);
}