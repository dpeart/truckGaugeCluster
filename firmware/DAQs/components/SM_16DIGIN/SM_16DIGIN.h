#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sequent_i2c.h"

#define SLAVE_OWN_ADDRESS_16DIGIN 0x20

#define INPUTS16_INPORT_REG_ADD   0x00
#define INPUTS16_OUTPORT_REG_ADD  0x02
#define INPUTS16_POLINV_REG_ADD   0x04
#define INPUTS16_CFG_REG_ADD      0x06

#define INPUTS16_CHANNEL_NR_MIN   1
#define INPUTS16_CHANNEL_NR_MAX   16

class SM_16DIGIN
{
public:
    SM_16DIGIN(uint8_t stack = 0);

    bool begin();
    bool isAlive();

    int  readInputs();            // bitmap of all 16 inputs
    bool readInputs(uint8_t ch);  // single input

private:
    uint8_t _hwAdd;
    bool    _detected;

    bool readBytes(uint8_t reg, uint8_t *buf, uint8_t len);
    int  readWord(uint8_t reg, uint16_t *value);
};
