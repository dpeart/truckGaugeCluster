#pragma once

#include <cstdint>
#include <cstdbool>
#include "driver/i2c.h"
#include "esp_err.h"

#define SLAVE_OWN_ADDRESS_16DIGIN 0x20

#define INPUTS16_INPORT_REG_ADD  0x00
#define INPUTS16_OUTPORT_REG_ADD 0x02
#define INPUTS16_POLINV_REG_ADD  0x04
#define INPUTS16_CFG_REG_ADD     0x06

#define INPUTS16_CHANNEL_NR_MIN  1
#define INPUTS16_CHANNEL_NR_MAX  16

class SM_16DIGIN {
public:
    /**
     * @brief Construct a new SM_16DIGIN object
     * @param i2c_port ESP-IDF I2C port number (e.g., I2C_NUM_0)
     * @param stack Stack level jumper setting [0..7]
     */
    SM_16DIGIN(i2c_port_t i2c_port = I2C_NUM_0, uint8_t stack = 0);

    /**
     * @brief Check card presence on the bus
     * @return true if detected, false otherwise
     */
    bool begin();

    /**
     * @brief Return card detection status
     * @return true if card is present
     */
    bool isAlive() const;

    /**
     * @brief Read digital ports as a 16-bit bitmap.
     * @return 16-bit state of all inputs, or negative value on error
     */
    int readInputs();

    /**
     * @brief Read one digital input channel.
     * @param channel [1..16]
     * @return true if pin is high/active, false otherwise
     */
    bool readInputs(uint8_t channel);

private:
    i2c_port_t _i2c_port;
    uint8_t _hwAdd;
    bool _detected;

    esp_err_t readWord(uint8_t reg_addr, uint16_t *value);
};