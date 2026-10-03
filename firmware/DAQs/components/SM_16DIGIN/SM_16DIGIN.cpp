#include "SM_16DIGIN.h"
#include "esp_i2c_helpers.h"

static const uint16_t pinMask[INPUTS16_CHANNEL_NR_MAX] = {
    0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100,
    0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001
};

SM_16DIGIN::SM_16DIGIN(i2c_port_t i2c_port, uint8_t stack)
    : _i2c_port(i2c_port), _detected(false) {
    if (stack > 7) {
        stack = 7;
    }
    // Calculates address based on stack jumpers (XOR logic matches original Sequent driver)
    _hwAdd = (SLAVE_OWN_ADDRESS_16DIGIN + stack) ^ 0x07;
}

bool SM_16DIGIN::begin() {
    uint16_t value = 0;
    if (readWord(INPUTS16_CFG_REG_ADD, &value) == ESP_OK) {
        _detected = true;
    } else {
        _detected = false;
    }
    return _detected;
}

bool SM_16DIGIN::isAlive() const {
    return _detected;
}

int SM_16DIGIN::readInputs() {
    uint16_t val = 0;
    if (readWord(INPUTS16_INPORT_REG_ADD, &val) != ESP_OK) {
        return -1;
    }

    int ret = 0;
    val = ~val; // Invert active-low logic
    for (int i = 0; i < INPUTS16_CHANNEL_NR_MAX; i++) {
        if (val & pinMask[i]) {
            ret |= (1 << i);
        }
    }
    return ret;
}

bool SM_16DIGIN::readInputs(uint8_t channel) {
    if (channel < INPUTS16_CHANNEL_NR_MIN || channel > INPUTS16_CHANNEL_NR_MAX) {
        return false;
    }

    uint16_t val = 0;
    if (readWord(INPUTS16_INPORT_REG_ADD, &val) != ESP_OK) {
        return false;
    }

    // Returns true if the channel bit is 0 (active low input)
    return ((val & pinMask[channel - 1]) == 0);
}

esp_err_t SM_16DIGIN::readWord(uint8_t reg_addr, uint16_t *value) {
    if (value == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[2] = {0};
    esp_err_t err = esp_i2c_read_reg(_i2c_port, _hwAdd, reg_addr, buf, 2);
    if (err == ESP_OK) {
        *value = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
    }
    return err;
}