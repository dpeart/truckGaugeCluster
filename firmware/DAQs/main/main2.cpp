#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_i2c_helpers.h"
#include "DFRobot_GNSS.h"

#include "Globals.h"
#include "SM_16DIGIN.h"

static const char *TAG = "GNSS_TEST";

static const char *SCANNER_TAG = "I2C_SCANNER";

/**
 * @brief Scans the specified I2C bus for connected devices and prints their addresses.
 *
 * @param port The I2C port number (e.g., I2C_NUM_0)
 */
void scan_i2c_bus(i2c_port_t port)
{
    ESP_LOGI(SCANNER_TAG, "Starting I2C bus scan...");
    uint8_t devices_found = 0;

    for (uint8_t addr = 1; addr < 127; addr++)
    {
        uint8_t dummy = 0;
        // Attempt a 1-byte read from register 0x00
        esp_err_t res = esp_i2c_read_reg(port, addr, 0x00, &dummy, 1);

        if (res == ESP_OK)
        {
            ESP_LOGI(SCANNER_TAG, "Found device at 7-bit address: 0x%02X (8-bit read: 0x%02X)",
                     addr, (addr << 1) | 1);
            devices_found++;
        }
    }

    if (devices_found == 0)
    {
        ESP_LOGW(SCANNER_TAG, "No I2C devices found. Check SDA/SCL pins, power, and pull-up resistors.");
    }
    else
    {
        ESP_LOGI(SCANNER_TAG, "Scan complete. Found %d device(s).", devices_found);
    }
}

// Callback function to process raw GNSS data (e.g., NMEA sentences)
void gnssDataCallback(char *data, uint8_t len)
{
    // Ensure string is null-terminated before printing
    char buffer[256] = {0};
    if (len < sizeof(buffer))
    {
        memcpy(buffer, data, len);
        printf("%s", buffer);
    }
}

// Stack 0 matches the default address logic or DIG_CARD from Globals.h
SM_16DIGIN digCard(I2C_MASTER_NUM, 0);

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C driver...");

    // 1. Initialize ESP-IDF I2C driver using helper
    esp_err_t err = esp_i2c_init(I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(err));
        return;
    }

    // 2. Instantiate GNSS object with default address (0x75)
    // DFRobot_GNSS_I2C gnss(I2C_MASTER_NUM, I2C_GNSS_ADR);

    // ESP_LOGI(TAG, "Checking GNSS module connection...");
    // while (!gnss.begin())
    // {
    //     ESP_LOGE(TAG, "GNSS module not detected. Retrying in 1s...");
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    // ESP_LOGI(TAG, "GNSS module detected!");

    // 3. Setup configurations
    // gnss.enablePower();
    // gnss.setGnss(eGPS_BeiDou);
    // gnss.setRgbOn();
    // gnss.setCallback(gnssDataCallback);

    // if (digCard.begin())
    // {
    //     printf("16DIGIN Card detected!\n");
    // }
    // else
    // {
    //     printf("16DIGIN Card not responding.\n");
    // }

    // 4. Main loop
    while (true)
    {
        // sTim_t utc = gnss.getUTC();
        // sTim_t date = gnss.getDate();
        // sLonLat_t lat = gnss.getLat();
        // sLonLat_t lon = gnss.getLon();
        // uint8_t sats = gnss.getNumSatUsed();

        ESP_LOGI(TAG, "-----------------------------------");
        // ESP_LOGI(TAG, "Date: %04d-%02d-%02d | UTC: %02d:%02d:%02d",
        //          date.year, date.month, date.date,
        //          utc.hour, utc.minute, utc.second);
        // ESP_LOGI(TAG, "Lat: %.6f ° %c | Lon: %.6f ° %c",
        //          lat.latitudeDegree, lat.latDirection,
        //          lon.lonitudeDegree, lon.lonDirection);
        // ESP_LOGI(TAG, "Satellites in Use: %d", sats);
        // ESP_LOGI(TAG, "Altitude: %.2f m | Speed: %.2f knots",
        //          gnss.getAlt(), gnss.getSog());

        // // Stream raw GNSS data via callback
        // ESP_LOGI(TAG, "Raw Data Stream:");
        // gnss.getAllGnss();

        // Reading specific channel using Globals.h defines
        // int inputsBitmap = digCard.readInputs();
        // printf("Inputs Bitmap: %d\n", inputsBitmap);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}