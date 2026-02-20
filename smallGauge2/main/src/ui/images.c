#include "images.h"

const ext_img_desc_t images[3] = {
    { "batteryVoltage", &img_battery_voltage },
    { "egt", &img_egt },
    { "iat", &img_iat },
};