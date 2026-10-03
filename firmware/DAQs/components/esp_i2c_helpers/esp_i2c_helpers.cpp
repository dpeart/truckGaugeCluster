#include "esp_i2c_helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

esp_err_t esp_i2c_init(i2c_port_t i2c_port,
                       gpio_num_t sda,
                       gpio_num_t scl,
                       uint32_t freq) {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = freq;
    conf.clk_flags = 0;

    esp_err_t err = i2c_param_config(i2c_port, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
}

esp_err_t esp_i2c_write_reg(i2c_port_t i2c_port,
                            uint8_t dev_addr,
                            uint8_t reg,
                            const uint8_t *data,
                            size_t len) {
    uint8_t buf[256];
    if (len + 1 > sizeof(buf)) {
        return ESP_ERR_INVALID_ARG;
    }
    buf[0] = reg;
    if (data && len > 0) {
        std::memcpy(&buf[1], data, len);
    }
    return i2c_master_write_to_device(i2c_port, dev_addr, buf, len + 1, pdMS_TO_TICKS(1000));
}

esp_err_t esp_i2c_read_reg(i2c_port_t i2c_port,
                           uint8_t dev_addr,
                           uint8_t reg,
                           uint8_t *data,
                           size_t len) {
    esp_err_t err = i2c_master_write_to_device(i2c_port, dev_addr, &reg, 1, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        return err;
    }
    return i2c_master_read_from_device(i2c_port, dev_addr, data, len, pdMS_TO_TICKS(1000));
}