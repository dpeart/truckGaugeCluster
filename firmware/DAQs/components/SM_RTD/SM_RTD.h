#pragma once

#include <cstdint>
#include <cstdbool>
#include "driver/i2c.h"
#include "esp_err.h"

#define RTD_CHANNEL_NR_MIN      1
#define RTD_CHANNEL_NR_MAX      8
#define RTD_SLAVE_OWN_ADDRESS_BASE 0x40

enum {
    RTD_VAL1_ADD = 0,
    RTD_VAL2_ADD = RTD_VAL1_ADD + 4,
    RTD_VAL3_ADD = RTD_VAL2_ADD + 4,
    RTD_VAL4_ADD = RTD_VAL3_ADD + 4,
    RTD_VAL5_ADD = RTD_VAL4_ADD + 4,
    RTD_VAL6_ADD = RTD_VAL5_ADD + 4,
    RTD_VAL7_ADD = RTD_VAL6_ADD + 4,
    RTD_VAL8_ADD = RTD_VAL7_ADD + 4,

    DIAG_TEMPERATURE_MEM_ADD = RTD_VAL8_ADD + 4,
    DIAG_5V_MEM_ADD,

    I2C_MEM_WDT_RESET_ADD = DIAG_5V_MEM_ADD + 2,
    I2C_MEM_WDT_INTERVAL_SET_ADD,
    I2C_MEM_WDT_INTERVAL_GET_ADD = I2C_MEM_WDT_INTERVAL_SET_ADD + 2,
    I2C_MEM_WDT_INIT_INTERVAL_SET_ADD = I2C_MEM_WDT_INTERVAL_GET_ADD + 2,
    I2C_MEM_WDT_INIT_INTERVAL_GET_ADD = I2C_MEM_WDT_INIT_INTERVAL_SET_ADD + 2,
    I2C_MEM_WDT_RESET_COUNT_ADD = I2C_MEM_WDT_INIT_INTERVAL_GET_ADD + 2,
    I2C_MEM_WDT_CLEAR_RESET_COUNT_ADD = I2C_MEM_WDT_RESET_COUNT_ADD + 2,
    I2C_MEM_WDT_POWER_OFF_INTERVAL_SET_ADD,
    I2C_MEM_WDT_POWER_OFF_INTERVAL_GET_ADD = I2C_MEM_WDT_POWER_OFF_INTERVAL_SET_ADD + 4,

    REVISION_HW_MAJOR_MEM_ADD = I2C_MEM_WDT_POWER_OFF_INTERVAL_GET_ADD + 4,
    REVISION_HW_MINOR_MEM_ADD,
    REVISION_MAJOR_MEM_ADD,
    REVISION_MINOR_MEM_ADD,

    RTD_RES1_ADD,
    RTD_RES2_ADD = RTD_RES1_ADD + 4,
    RTD_RES3_ADD = RTD_RES2_ADD + 4,
    RTD_RES4_ADD = RTD_RES3_ADD + 4,
    RTD_RES5_ADD = RTD_RES4_ADD + 4,
    RTD_RES6_ADD = RTD_RES5_ADD + 4,
    RTD_RES7_ADD = RTD_RES6_ADD + 4,
    RTD_RES8_ADD = RTD_RES7_ADD + 4,
};

class SM_RTD {
public:
    /**
     * @brief Construct a new SM_RTD object
     * @param i2c_port ESP-IDF I2C port (e.g. I2C_NUM_0)
     * @param stack Address jumper stack level [0..7]
     */
    SM_RTD(i2c_port_t i2c_port = I2C_NUM_0, uint8_t stack = 0);

    bool begin();
    bool isAlive() const;

    /**
     * @brief Read channel temperature in degrees Celsius
     * @param channel Channel number 1..8
     * @return Temperature in °C, or -1000.0f on error
     */
    float readTemp(uint8_t channel);

    /**
     * @brief Read channel resistance in Ohms
     * @param channel Channel number 1..8
     * @return Resistance in Ohms, or -1.0f on error
     */
    float readRes(uint8_t channel);

private:
    i2c_port_t _i2c_port;
    uint8_t _hwAdd;
    bool _detected;

    esp_err_t writeByte(uint8_t reg, uint8_t val);
    esp_err_t readByte(uint8_t reg, uint8_t *val);
    esp_err_t readFloat(uint8_t reg, float *val);
};