#include "Globals.h"

// Function to calculate pressure from voltage
int calculatePressure5PSI(float mV) {
    const float offset = 0.5;
    const float sensitivity = 0.0533;
    float psi ((((mV / 1000) - offset) / sensitivity) - 14.503);
    return int(psi * INT_SCALING);
};

int calculatePressure7PSI(float mV) {
    const float offset = 0.5;
    const float sensitivity = 0.04;
    float psi ((((mV / 1000) - offset) / sensitivity) - 14.503);
    return int(psi * INT_SCALING);
};

