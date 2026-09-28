extern "C" {
#include <inttypes.h>
}

#include "SM_16DIGIN.h"
#include "sequent_i2c.h"
#include <string.h>
#include "esp_err.h"

const uint16_t pinMask[INPUTS16_CHANNEL_NR_MAX] = {
    0x8000, 0x4000, 0x2000, 0x1000,
    0x0800, 0x0400, 0x0200, 0x0100,
    0x0080, 0x0040, 0x0020, 0x0010,
    0x0008, 0x0004, 0x0002, 0x0001
};

SM_16DIGIN::SM_16DIGIN(uint8_t stack)
{
    if (stack > 7)
        stack = 7;

    _hwAdd = (SLAVE_OWN_ADDRESS_16DIGIN + stack) ^ 0x07;
    _detected = false;
}

bool SM_16DIGIN::begin()
{
    uint16_t value = 0;

    if (readWord(INPUTS16_CFG_REG_ADD, &value) == 0)
        _detected = true;

    return _detected;
}

bool SM_16DIGIN::isAlive()
{
    return _detected;
}

int SM_16DIGIN::readInputs()
{
    uint16_t val = 0;

    if (readWord(INPUTS16_INPORT_REG_ADD, &val) < 0)
        return -1;

    val = ~val;

    int ret = 0;
    for (int i = 0; i < INPUTS16_CHANNEL_NR_MAX; i++)
    {
        if (val & pinMask[i])
            ret |= (1 << i);
    }

    return ret;
}

bool SM_16DIGIN::readInputs(uint8_t channel)
{
    if (channel < 1 || channel > INPUTS16_CHANNEL_NR_MAX)
        return false;

    uint16_t val = 0;

    if (readWord(INPUTS16_INPORT_REG_ADD, &val) < 0)
        return false;

    return ((val & pinMask[channel - 1]) == 0);
}

int SM_16DIGIN::readWord(uint8_t add, uint16_t *value)
{
    if (!value)
        return -1;

    uint8_t buff[2];

    if (sequent_i2c_read(_hwAdd, add, buff, 2) != ESP_OK)
        return -1;

    memcpy(value, buff, 2);
    return 0;
}
