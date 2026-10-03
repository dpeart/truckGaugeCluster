#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Globals.h"
#include "esp_i2c_helpers.h"
#include "SM_RTD.h"

static const char *TAG = "RTD_TEST";

// Instantiate RTD Card (Stack 0 uses default address 0x40)
SM_RTD rtdCard(I2C_MASTER_NUM, 0);

/**
 * @brief FreeRTOS Task to scan all 8 RTD channels sequentially
 */
void rtd_scan_task(void *pvParameters) {
    uint8_t channel = 1;

    while (1) {
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

        // Cycle through channels 1 to 8
        channel++;
        if (channel > RTD_CHANNEL_NR_MAX) {
            channel = 1;
            ESP_LOGI(TAG, "---------------------------------------------");
        }

        // Delay 250ms between readings
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

    // 2. Initialize RTD Card
    if (rtdCard.begin()) {
        ESP_LOGI(TAG, "SM_RTD Card detected at address 0x40");
    } else {
        ESP_LOGW(TAG, "SM_RTD Card not detected at address 0x40!");
    }

    // 3. Create scan task
    xTaskCreate(rtd_scan_task, "rtd_scan_task", 4096, NULL, 5, NULL);
}