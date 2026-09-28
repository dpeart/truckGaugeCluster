#include "sequent_i2c.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SEQUENT_I2C";

int sequent_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) return -1;

    // Phase 1: Write target base register address with STOP condition
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(SEQUENT_I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write reg 0x%02X to 0x%02X failed: %s", reg, addr, esp_err_to_name(ret));
        return -1;
    }

    // Phase 2: Execution delay for STM8 coprocessor pointer setup
    esp_rom_delay_us(100);

    // Phase 3: Read data payload burst
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(SEQUENT_I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read %d bytes from 0x%02X failed: %s", len, addr, esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int sequent_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    if (data && len > 0) {
        i2c_master_write(cmd, (uint8_t *)data, len, true);
    }

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(SEQUENT_I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write to 0x%02X reg 0x%02X failed: %s", addr, reg, esp_err_to_name(ret));
    }

    // Coprocessor state-machine recovery delay
    vTaskDelay(pdMS_TO_TICKS(1));

    return (ret == ESP_OK) ? 0 : -1;
}