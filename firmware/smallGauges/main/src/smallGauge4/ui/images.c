#include "images.h"

const ext_img_desc_t images[4] = {
    { "fuelPressure", &img_fuel_pressure },
    { "transTemp", &img_trans_temp },
    { "oilPressure", &img_oil_pressure },
    { "turbo", &img_turbo },
};