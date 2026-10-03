#include "SM_RTD.h"
#include "esp_i2c_helpers.h"
#include <cstring>

SM_RTD::SM_RTD(i2c_port_t i2c_port, uint8_t stack)
    : _i2c_port(i2c_port), _detected(false) {
    if (stack > 7) stack = 7;
    _hwAdd = RTD_SLAVE_OWN_ADDRESS_BASE + stack;
}

bool SM_RTD::begin() {
    uint8_t rev = 0;
    if (readByte(REVISION_MAJOR_MEM_ADD, &rev) == ESP_OK) {
        _detected = true;
    } else {
        _detected = false;
    }
    return _detected;
}

bool SM_RTD::isAlive() const {
    return _detected;
}

float SM_RTD::readTemp(uint8_t channel) {
    if (channel < RTD_CHANNEL_NR_MIN || channel > RTD_CHANNEL_NR_MAX) {
        return -1000.0f;
    }

    float val = -1000.0f;
    uint8_t reg = RTD_VAL1_ADD + sizeof(float) * (channel - 1);
    if (readFloat(reg, &val) != ESP_OK) {
        return -1001.0f;
    }
    return val;
}

float SM_RTD::readRes(uint8_t channel) {
    if (channel < RTD_CHANNEL_NR_MIN || channel > RTD_CHANNEL_NR_MAX) {
        return -1.0f;
    }

    float val = -1000.0f;
    uint8_t reg = RTD_RES1_ADD + sizeof(float) * (channel - 1);
    if (readFloat(reg, &val) != ESP_OK) {
        return -2.0f;
    }
    return val;
}

// Helper methods utilizing esp_i2c_helpers
esp_err_t SM_RTD::writeByte(uint8_t reg, uint8_t val) {
    return esp_i2c_write_reg(_i2c_port, _hwAdd, reg, &val, 1);
}

esp_err_t SM_RTD::readByte(uint8_t reg, uint8_t *val) {
    return esp_i2c_read_reg(_i2c_port, _hwAdd, reg, val, 1);
}

esp_err_t SM_RTD::readFloat(uint8_t reg, float *val) {
    uint8_t buf[4] = {0};
    esp_err_t err = esp_i2c_read_reg(_i2c_port, _hwAdd, reg, buf, 4);
    if (err == ESP_OK) {
        std::memcpy(val, buf, 4);
    }
    return err;
}