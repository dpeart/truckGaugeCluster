#include "SM_16UNIVIN.h"
#include "esp_i2c_helpers.h"
#include <cstring>

SM_16_UNIVIN::SM_16_UNIVIN(i2c_port_t i2c_port, uint8_t stack)
    : _i2c_port(i2c_port), _detected(false) {
    if (stack > 7) stack = 7;
    _hwAdd = UI_SLAVE_OWN_ADDRESS_BASE + stack;
}

bool SM_16_UNIVIN::begin() {
    uint8_t rev = 0;
    if (readByte(UI_I2C_MEM_REVISION_MAJOR_ADD, &rev) == ESP_OK) {
        _detected = true;
    } else {
        _detected = false;
    }
    return _detected;
}

bool SM_16_UNIVIN::isAlive() const {
    return _detected;
}

bool SM_16_UNIVIN::writeLED(uint8_t led, bool val) {
    if (led == 0 || led > UI_UNIV_CH_NR) return false;
    uint8_t reg = val ? UI_I2C_MEM_LED_SET : UI_I2C_MEM_LED_CLR;
    return (writeByte(reg, led) == ESP_OK);
}

bool SM_16_UNIVIN::writeLED(uint16_t val) {
    return (writeWord(UI_I2C_MEM_LEDS, val) == ESP_OK);
}

int SM_16_UNIVIN::readAnalogMv(uint8_t channel) {
    if (channel < 1 || channel > UI_UNIV_CH_NR) return -1;
    uint16_t val = 0;
    uint8_t reg = UI_I2C_U0_10_IN_VAL1_ADD + (channel - 1) * UI_ANALOG_VAL_SIZE;
    if (readWord(reg, &val) != ESP_OK) return -1;
    return static_cast<int>(val);
}

int SM_16_UNIVIN::readRes1k(uint8_t channel) {
    if (channel < 1 || channel > UI_UNIV_CH_NR) return -1;
    uint16_t val = 0;
    uint8_t reg = UI_I2C_R_1K_CH1 + (channel - 1) * UI_ANALOG_VAL_SIZE;
    if (readWord(reg, &val) != ESP_OK) return -1;
    return static_cast<int>(val);
}

int SM_16_UNIVIN::readRes10k(uint8_t channel) {
    if (channel < 1 || channel > UI_UNIV_CH_NR) return -1;
    uint16_t val = 0;
    uint8_t reg = UI_I2C_R_10K_CH1 + (channel - 1) * UI_ANALOG_VAL_SIZE;
    if (readWord(reg, &val) != ESP_OK) return -1;
    return static_cast<int>(val);
}

bool SM_16_UNIVIN::readDC(uint8_t channel) {
    if (channel < 1 || channel > UI_UNIV_CH_NR) return false;
    uint16_t aux = 0;
    if (readWord(UI_I2C_MEM_DRY_CONTACT, &aux) == ESP_OK) {
        return (aux & (1 << (channel - 1))) != 0;
    }
    return false;
}

int SM_16_UNIVIN::readDC() {
    uint16_t aux = 0;
    if (readWord(UI_I2C_MEM_DRY_CONTACT, &aux) == ESP_OK) {
        return static_cast<int>(aux);
    }
    return -1;
}

bool SM_16_UNIVIN::cfgCounter(uint8_t channel, bool enable) {
    if (channel < 1 || channel > UI_UNIV_CH_NR) return false;
    uint16_t aux = 0;
    if (readWord(UI_I2C_MEM_DC_CNT_ENABLE, &aux) != ESP_OK) return false;

    if (enable) {
        aux |= (1 << (channel - 1));
    } else {
        aux &= ~(1 << (channel - 1));
    }
    return (writeWord(UI_I2C_MEM_DC_CNT_ENABLE, aux) == ESP_OK);
}

int SM_16_UNIVIN::readCounter(uint8_t channel) {
    if (channel < 1 || channel > UI_UNIV_CH_NR) return -1;
    uint32_t val = 0;
    uint8_t reg = UI_I2C_MEM_DC_CNT_ADD + (channel - 1) * UI_COUNTER_SIZE;
    if (readDWord(reg, &val) != ESP_OK) return -1;
    return static_cast<int>(val);
}

bool SM_16_UNIVIN::resetCounter(uint8_t channel) {
    if (channel < 1 || channel > UI_UNIV_CH_NR) return false;
    return (writeByte(UI_I2C_MEM_DC_CNT_RST_ADD, channel) == ESP_OK);
}

// Low-level helper methods using standard esp_i2c_helpers
esp_err_t SM_16_UNIVIN::writeByte(uint8_t reg, uint8_t val) {
    return esp_i2c_write_reg(_i2c_port, _hwAdd, reg, &val, 1);
}

esp_err_t SM_16_UNIVIN::writeWord(uint8_t reg, uint16_t val) {
    uint8_t buf[2];
    std::memcpy(buf, &val, 2);
    return esp_i2c_write_reg(_i2c_port, _hwAdd, reg, buf, 2);
}

esp_err_t SM_16_UNIVIN::readByte(uint8_t reg, uint8_t *val) {
    return esp_i2c_read_reg(_i2c_port, _hwAdd, reg, val, 1);
}

esp_err_t SM_16_UNIVIN::readWord(uint8_t reg, uint16_t *val) {
    uint8_t buf[2] = {0};
    esp_err_t err = esp_i2c_read_reg(_i2c_port, _hwAdd, reg, buf, 2);
    if (err == ESP_OK) {
        *val = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
    }
    return err;
}

esp_err_t SM_16_UNIVIN::readDWord(uint8_t reg, uint32_t *val) {
    uint8_t buf[4] = {0};
    esp_err_t err = esp_i2c_read_reg(_i2c_port, _hwAdd, reg, buf, 4);
    if (err == ESP_OK) {
        *val = static_cast<uint32_t>(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
    }
    return err;
}