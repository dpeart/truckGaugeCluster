#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Globals.h"
#include "esp_i2c_helpers.h"
#include "SM_16UNIVIN.h"

static const char *TAG = "ADC_TEST";

// Mappings from Globals.h for logging output
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

// Instantiate ADC Card (Stack 0 uses default address 0x58)
SM_16_UNIVIN adcCard(I2C_MASTER_NUM, 0);

/**
 * @brief FreeRTOS Task to scan ADC channels sequentially
 */
void adc_scan_task(void *pvParameters) {
    size_t adc_idx = 0;
    const size_t total_adc = sizeof(adcInputs) / sizeof(adcInputs[0]);

    while (1) {
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

        // Advance to next channel in the sequence
        adc_idx++;
        if (adc_idx >= total_adc) {
            adc_idx = 0;
            ESP_LOGI(TAG, "---------------------------------------------");
        }

        // 250ms delay between readings
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Initializing I2C Master Port...");

    // 1. Initialize shared I2C bus helper
    esp_err_t err = esp_i2c_init(I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus driver!");
        return;
    }
    ESP_LOGI(TAG, "I2C initialized successfully.");

    // 2. Initialize ADC / Universal Input Card
    if (adcCard.begin()) {
        ESP_LOGI(TAG, "16UNIVIN Card detected at address 0x%02X", ADC_CARD);
    } else {
        ESP_LOGW(TAG, "16UNIVIN Card not detected at address 0x%02X!", ADC_CARD);
    }

    // 3. Create scan task
    xTaskCreate(adc_scan_task, "adc_scan_task", 4096, NULL, 5, NULL);
}