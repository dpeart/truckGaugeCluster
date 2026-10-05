#include "Globals.h"


/**
 * Calculates Gauge Pressure (PSIG) from a 5 Bar Absolute Transducer (0.5V - 4.5V = 0 to 5 Bar PSIA)
 * @param mV Raw input voltage in millivolts from ADC.
 * @param ambientBaroPsi Dynamic atmospheric pressure in PSIA (from GPS or Goshen default).
 */
int calculatePressure5BAR(float mV, float ambientBaroPsi) {
    const float offset = 0.575f;
    const float sensitivity = 0.055157f; // 4.0V / 72.519 PSIA (5 Bar)

    // 1. Calculate Absolute Pressure (PSIA)
    float psia = ((mV / 1000.0f) - offset) / sensitivity;

    // 2. Subtract dynamic barometric pressure to get Gauge Pressure (PSIG)
    float psig = psia - ambientBaroPsi;

    // 3. Clamp noise/vacuum below zero
    if (psig < 0.0f) {
        psig = 0.0f;
    }

    return static_cast<int>(psig * INT_SCALING);
}

int calculatePressure7PSI(float mV) {
    const float offset = 0.5;
    const float sensitivity = 0.04;
    float psi ((((mV / 1000) - offset) / sensitivity) - 14.503);
    return int(psi * INT_SCALING);
};

