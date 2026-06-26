#include "images.h"

const ext_img_desc_t images[4] = {
    { "oilPressure", &img_oil_pressure },
    { "engineTemp", &img_engine_temp },
    { "fuelLocation", &img_fuel_location },
    { "center", &img_center },
};