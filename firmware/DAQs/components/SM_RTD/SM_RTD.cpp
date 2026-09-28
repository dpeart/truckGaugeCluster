extern "C" {
#include <inttypes.h>
}

#include "SM_RTD.h"
#include "sequent_i2c.h"
#include <string.h>
#include "esp_err.h"

SM_RTD::SM_RTD(uint8_t stack)
{
    if (stack > 7)
        stack = 7;

    _hwAdd = SLAVE_OWN_ADDRESS_BASE + stack;
    _detected = false;
}

bool SM_RTD::begin()
{
    uint8_t value = 0;

    if (readByte(REVISION_MAJOR_MEM_ADD, &value) == 0)
        _detected = true;

    return _detected;
}

bool SM_RTD::isAlive()
{
    return _detected;
}

float SM_RTD::readTemp(uint8_t channel)
{
    if (channel < 1 || channel > RTD_CHANNEL_NR_MAX)
        return -1000.0f;

    float val = -1000.0f;

    if (readFloat(RTD_VAL1_ADD + sizeof(float) * (channel - 1), &val) != ESP_OK)
        return -1001.0f;

    return val;
}

float SM_RTD::readRes(uint8_t channel)
{
    if (channel < 1 || channel > RTD_CHANNEL_NR_MAX)
        return -1.0f;

    float val = -1.0f;

    if (readFloat(RTD_RES1_ADD + sizeof(float) * (channel - 1), &val) != ESP_OK)
        return -2.0f;

    return val;
}

// -------------------- I2C ACCESS --------------------

int SM_RTD::writeByte(uint8_t add, uint8_t value)
{
    return sequent_i2c_write(_hwAdd, add, &value, 1);
}

int SM_RTD::readByte(uint8_t add, uint8_t *value)
{
    if (!value)
        return -1;

    return sequent_i2c_read(_hwAdd, add, value, 1);
}

int SM_RTD::readFloat(uint8_t add, float *value)
{
    if (!value)
        return -1;

    uint8_t buff[4];

    if (sequent_i2c_read(_hwAdd, add, buff, 4) !=  ESP_OK)
        return -1;

    memcpy(value, buff, 4);
    return 0;
}
