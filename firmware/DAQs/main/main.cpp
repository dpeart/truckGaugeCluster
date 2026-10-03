#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"

#include "esp_i2c_helpers.h"
#include "mcp960x.h"

static const char *TAG = "MAIN_TEST";

// Pin assignments for Sequent / ESP32 board
#define I2C_MASTER_SCL_IO           22     // Adjust to your board's SCL pin
#define I2C_MASTER_SDA_IO           21     // Adjust to your board's SDA pin
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000 // 100 kHz standard speed

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C Master...");
    ESP_ERROR_CHECK(i2c_master_init());

    mcp960x_t dev;
    // Initialize descriptor for default address (0x67) on I2C_NUM_0
    ESP_ERROR_CHECK(mcp960x_init_desc(&dev, MCP960X_ADDR_DEFAULT, I2C_MASTER_NUM));

    ESP_LOGI(TAG, "Initializing MCP960X device...");
    esp_err_t err = mcp960x_init(&dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to MCP960X at address 0x%02X (Error: %s)", dev.addr, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "MCP960X detected! Device ID: 0x%02X, Revision: 0x%02X", dev.id, dev.revision);

    // Configure sensor: Type K Thermocouple, Filter Level 0 (Off)
    ESP_ERROR_CHECK(mcp960x_set_sensor_config(&dev, MCP960X_TYPE_K, MCP960X_FILTER_OFF));

    // Configure device mode: Normal mode, 1 sample burst, 18-bit ADC resolution, 0.0625°C CJ resolution
    ESP_ERROR_CHECK(mcp960x_set_device_config(&dev, 
                                            MCP960X_MODE_NORMAL, 
                                            MCP960X_SAMPLES_1, 
                                            MCP960X_ADC_RES_18, 
                                            MCP960X_TC_RES_0_0625));

    ESP_LOGI(TAG, "Starting temperature read loop...\n");

    while (1) {
        float hot_temp = 0.0f;
        float cold_temp = 0.0f;
        float delta_temp = 0.0f;
        bool temp_ready = false;
        mcp960x_status_t dev_status = MCP960X_OK;

        // Check readiness and circuit status
        if (mcp960x_get_status(&dev, &temp_ready, NULL, &dev_status, NULL, NULL, NULL, NULL) == ESP_OK) {
            if (dev_status == MCP960X_OPEN_CIRCUIT) {
                ESP_LOGW(TAG, "Warning: Thermocouple open circuit detected!");
            } else if (dev_status == MCP960X_SHORT_CIRCUIT) {
                ESP_LOGW(TAG, "Warning: Thermocouple short circuit detected!");
            }
        }

        // Read temperatures
        esp_err_t res_hot = mcp960x_get_thermocouple_temp(&dev, &hot_temp);
        esp_err_t res_cold = mcp960x_get_ambient_temp(&dev, &cold_temp);
        esp_err_t res_delta = mcp960x_get_delta_temp(&dev, &delta_temp);

        if (res_hot == ESP_OK && res_cold == ESP_OK && res_delta == ESP_OK) {
            ESP_LOGI(TAG, "Hot Junction: %6.2f °C | Cold Junction: %6.2f °C | Delta: %6.2f °C", 
                     hot_temp, cold_temp, delta_temp);
        } else {
            ESP_LOGE(TAG, "Failed to read temperature data from MCP960X");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}