#pragma once

#include <cstdint>
#include <cstdbool>
#include "driver/i2c.h"
#include "esp_err.h"

#define UI_UNIV_CH_NR         16
#define UI_ANALOG_VAL_SIZE    2
#define UI_COUNTER_SIZE       4
#define UI_SLAVE_OWN_ADDRESS_BASE 0x58

enum {
    UI_I2C_MEM_LEDS = 0,
    UI_I2C_MEM_LED_SET = UI_I2C_MEM_LEDS + 2,
    UI_I2C_MEM_LED_CLR,

    UI_I2C_MEM_DRY_CONTACT,

    UI_I2C_U0_10_IN_VAL1_ADD = UI_I2C_MEM_DRY_CONTACT + 2,

    UI_I2C_R_1K_CH1 = UI_I2C_U0_10_IN_VAL1_ADD + UI_UNIV_CH_NR * UI_ANALOG_VAL_SIZE,
    UI_I2C_R_10K_CH1 = UI_I2C_R_1K_CH1 + UI_UNIV_CH_NR * UI_ANALOG_VAL_SIZE,

    UI_I2C_MEM_DIAG_TEMPERATURE_ADD = UI_I2C_R_10K_CH1 + UI_UNIV_CH_NR * UI_ANALOG_VAL_SIZE,
    UI_I2C_MEM_DC_CNT_ENABLE,

    UI_I2C_MEM_DC_CNT_RST_ADD = UI_I2C_MEM_DC_CNT_ENABLE + 2,

    UI_I2C_MEM_DC_CNT_ADD,
    UI_I2C_MEM_DC_CNT_END_ADD = UI_I2C_MEM_DC_CNT_ADD + UI_UNIV_CH_NR * UI_COUNTER_SIZE,

    UI_I2C_MEM_REVISION_HW_MAJOR_ADD = 0x33,
    UI_I2C_MEM_REVISION_HW_MINOR_ADD,
    UI_I2C_MEM_REVISION_MAJOR_ADD,
    UI_I2C_MEM_REVISION_MINOR_ADD,
};

class SM_16_UNIVIN {
public:
    /**
     * @brief Construct a new SM_16_UNIVIN object
     * @param i2c_port ESP-IDF I2C port (e.g. I2C_NUM_0)
     * @param stack Address jumper stack level [0..7]
     */
    SM_16_UNIVIN(i2c_port_t i2c_port = I2C_NUM_0, uint8_t stack = 0);

    bool begin();
    bool isAlive() const;

    // LED controls
    bool writeLED(uint8_t led, bool val);
    bool writeLED(uint16_t val);

    // Analog readings
    int readAnalogMv(uint8_t channel);
    int readRes1k(uint8_t channel);
    int readRes10k(uint8_t channel);

    // Dry contact input readings
    bool readDC(uint8_t channel);
    int readDC();

    // Pulse counter functions
    bool cfgCounter(uint8_t channel, bool enable);
    int readCounter(uint8_t channel);
    bool resetCounter(uint8_t channel);

private:
    i2c_port_t _i2c_port;
    uint8_t _hwAdd;
    bool _detected;

    esp_err_t writeByte(uint8_t reg, uint8_t val);
    esp_err_t writeWord(uint8_t reg, uint16_t val);
    esp_err_t readByte(uint8_t reg, uint8_t *val);
    esp_err_t readWord(uint8_t reg, uint16_t *val);
    esp_err_t readDWord(uint8_t reg, uint32_t *val);
};