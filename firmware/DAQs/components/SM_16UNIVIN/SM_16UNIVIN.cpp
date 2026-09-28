extern "C" {
#include <inttypes.h>
}

#include "SM_16UNIVIN.h"
#include "sequent_i2c.h"
#include <string.h>
#include "esp_err.h"

SM_16_UNIVIN::SM_16_UNIVIN(uint8_t stack)
{
    if (stack > 7)
        stack = 7;

    _hwAdd = UI_SLAVE_OWN_ADDRESS_BASE + stack;
    _detected = false;
}

bool SM_16_UNIVIN::begin()
{
    uint8_t value = 0;

    if (readByte(UI_I2C_MEM_REVISION_MAJOR_ADD, &value) == 0)
        _detected = true;

    return _detected;
}

bool SM_16_UNIVIN::isAlive()
{
    return _detected;
}

bool SM_16_UNIVIN::writeLED(uint8_t led, bool val)
{
    if (led < 1 || led > UI_UNIV_CH_NR)
        return false;

    uint8_t cmd = val ? UI_I2C_MEM_LED_SET : UI_I2C_MEM_LED_CLR;

    return (writeByte(cmd, led) == ESP_OK);
}

bool SM_16_UNIVIN::writeLED(uint16_t val)
{
    return (writeWord(UI_I2C_MEM_LEDS, val) == ESP_OK);
}

int SM_16_UNIVIN::readAnalogMv(uint8_t channel)
{
    if (channel < 1 || channel > UI_UNIV_CH_NR)
        return -1;

    uint16_t val = 0;

    if (readWord(UI_I2C_U0_10_IN_VAL1_ADD + (channel - 1) * UI_ANALOG_VAL_SIZE, &val) < 0)
        return -1;

    return val;
}

int SM_16_UNIVIN::readRes1k(uint8_t channel)
{
    if (channel < 1 || channel > UI_UNIV_CH_NR)
        return -1;

    uint16_t val = 0;

    if (readWord(UI_I2C_R_1K_CH1 + (channel - 1) * UI_ANALOG_VAL_SIZE, &val) < 0)
        return -1;

    return val;
}

int SM_16_UNIVIN::readRes10k(uint8_t channel)
{
    if (channel < 1 || channel > UI_UNIV_CH_NR)
        return -1;

    uint16_t val = 0;

    if (readWord(UI_I2C_R_10K_CH1 + (channel - 1) * UI_ANALOG_VAL_SIZE, &val) < 0)
        return -1;

    return val;
}

bool SM_16_UNIVIN::readDC(uint8_t channel)
{
    if (channel < 1 || channel > UI_UNIV_CH_NR)
        return -1;

    uint16_t aux = 0;

    if (readWord(UI_I2C_MEM_DRY_CONTACT, &aux) != ESP_OK)
        return -1;

    return (aux & (1 << (channel - 1))) != 0;
}

int SM_16_UNIVIN::readDC()
{
    uint16_t aux = 0;

    if (readWord(UI_I2C_MEM_DRY_CONTACT, &aux) != ESP_OK)
        return -1;

    return aux;
}

bool SM_16_UNIVIN::cfgCounter(uint8_t channel, bool enable)
{
    if (channel < 1 || channel > UI_UNIV_CH_NR)
        return false;

    uint16_t aux = 0;

    if (readWord(UI_I2C_MEM_DC_CNT_ENABLE, &aux) != ESP_OK)
        return false;

    if (enable)
        aux |= (1 << (channel - 1));
    else
        aux &= ~(1 << (channel - 1));

    return (writeWord(UI_I2C_MEM_DC_CNT_ENABLE, aux) == ESP_OK);
}

int SM_16_UNIVIN::readCounter(uint8_t channel)
{
    if (channel < 1 || channel > UI_UNIV_CH_NR)
        return -1;

    uint32_t val = 0;

    if (readDWord(UI_I2C_MEM_DC_CNT_ADD + (channel - 1) * UI_COUNTER_SIZE, &val) != ESP_OK)
        return -1;

    return val;
}

bool SM_16_UNIVIN::resetCounter(uint8_t channel)
{
    if (channel < 1 || channel > UI_UNIV_CH_NR)
        return false;

    return (writeByte(UI_I2C_MEM_DC_CNT_RST_ADD, channel) == ESP_OK);
}

// -------------------- I2C ACCESS --------------------

int SM_16_UNIVIN::writeByte(uint8_t add, uint8_t value)
{
    return sequent_i2c_write(_hwAdd, add, &value, 1);
}

int SM_16_UNIVIN::writeWord(uint8_t add, uint16_t value)
{
    uint8_t buff[2];
    memcpy(buff, &value, 2);

    return sequent_i2c_write(_hwAdd, add, buff, 2);
}

int SM_16_UNIVIN::writeDWord(uint8_t add, uint32_t value)
{
    uint8_t buff[4];
    memcpy(buff, &value, 4);

    return sequent_i2c_write(_hwAdd, add, buff, 4);
}

int SM_16_UNIVIN::readByte(uint8_t add, uint8_t *value)
{
    if (!value)
        return -1;

    return sequent_i2c_read(_hwAdd, add, value, 1);
}

int SM_16_UNIVIN::readWord(uint8_t add, uint16_t *value)
{
    if (!value)
        return -1;

    uint8_t buff[2];

    if (sequent_i2c_read(_hwAdd, add, buff, 2) != ESP_OK)
        return -1;

    memcpy(value, buff, 2);
    return 0;
}

int SM_16_UNIVIN::readDWord(uint8_t add, uint32_t *value)
{
    if (!value)
        return -1;

    uint8_t buff[4];

    if (sequent_i2c_read(_hwAdd, add, buff, 4) != ESP_OK)
        return -1;

    memcpy(value, buff, 4);
    return 0;
}

int SM_16_UNIVIN::readSignedDWord(uint8_t add, int32_t *value)
{
    if (!value)
        return -1;

    uint8_t buff[4];

    if (sequent_i2c_read(_hwAdd, add, buff, 4) != ESP_OK)
        return -1;

    memcpy(value, buff, 4);
    return 0;
}
