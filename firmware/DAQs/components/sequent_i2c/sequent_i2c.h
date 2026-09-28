#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEQUENT_I2C_PORT I2C_NUM_0

int sequent_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
int sequent_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
