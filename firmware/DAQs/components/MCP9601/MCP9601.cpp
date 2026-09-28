#include "MCP9601.h"
#include "sequent_i2c.h"
#include <string.h>
#include "esp_err.h"

// MCP9601 Registers
#define MCP9601_REG_HOT_JUNCTION     0x00
#define MCP9601_REG_DELTA            0x01
#define MCP9601_REG_COLD_JUNCTION    0x02
#define MCP9601_REG_STATUS           0x04
#define MCP9601_REG_SENSOR_CONFIG    0x05
#define MCP9601_REG_DEVICE_CONFIG    0x06

// Thermocouple types (Adafruit style)
#define MCP9600_TYPE_K 0x00
#define MCP9600_TYPE_J 0x01
#define MCP9600_TYPE_T 0x02
#define MCP9600_TYPE_N 0x03
#define MCP9600_TYPE_S 0x04
#define MCP9600_TYPE_E 0x05
#define MCP9600_TYPE_B 0x06
#define MCP9600_TYPE_R 0x07

MCP9601::MCP9601(uint8_t address)
{
    _addr = address;
    _detected = false;
}

bool MCP9601::begin()
{
    uint8_t status = 0;

    if (read8(MCP9601_REG_STATUS, &status) == 0)
        _detected = true;

    return _detected;
}

bool MCP9601::isAlive()
{
    return _detected;
}

void MCP9601::setThermocoupleType(uint8_t type)
{
    write8(MCP9601_REG_SENSOR_CONFIG, type & 0x07);
}

float MCP9601::readThermocouple()
{
    int32_t raw = 0;

    if (read24(MCP9601_REG_HOT_JUNCTION, &raw) != 0)
        return -1000.0f;

    raw >>= 5;  // 19-bit signed value

    return raw * 0.0078125f;  // 1/128 °C per LSB
}

// -------------------- I2C Helpers --------------------

int MCP9601::read8(uint8_t reg, uint8_t *val)
{
    return sequent_i2c_read(_addr, reg, val, 1);
}

int MCP9601::read16(uint8_t reg, int16_t *val)
{
    uint8_t buf[2];

    if (sequent_i2c_read(_addr, reg, buf, 2) != ESP_OK)
        return -1;

    *val = (int16_t)((buf[0] << 8) | buf[1]);
    return 0;
}

int MCP9601::read24(uint8_t reg, int32_t *val)
{
    uint8_t buf[3];

    if (sequent_i2c_read(_addr, reg, buf, 3) != ESP_OK)
        return -1;

    *val = (int32_t)((buf[0] << 16) | (buf[1] << 8) | buf[2]);
    return 0;
}

int MCP9601::write8(uint8_t reg, uint8_t v)
{
    return sequent_i2c_write(_addr, reg, &v, 1);
}
