#pragma once
#include <stdint.h>

class MCP9601 {
public:
    MCP9601(uint8_t address);

    bool begin();
    bool isAlive();

    void setThermocoupleType(uint8_t type);
    float readThermocouple();

private:
    uint8_t _addr;
    bool _detected;

    int read8(uint8_t reg, uint8_t *val);
    int read16(uint8_t reg, int16_t *val);
    int read24(uint8_t reg, int32_t *val);
    int write8(uint8_t reg, uint8_t val);
};
