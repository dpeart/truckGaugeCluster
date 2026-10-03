#pragma once
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_i2c_init(i2c_port_t i2c_port,
                       gpio_num_t sda,
                       gpio_num_t scl,
                       uint32_t freq);

esp_err_t esp_i2c_write_reg(i2c_port_t i2c_port,
                            uint8_t dev_addr,
                            uint8_t reg,
                            const uint8_t *data,
                            size_t len);

esp_err_t esp_i2c_read_reg(i2c_port_t i2c_port,
                           uint8_t dev_addr,
                           uint8_t reg,
                           uint8_t *data,
                           size_t len);

#ifdef __cplusplus
}
#endif