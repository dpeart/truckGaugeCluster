#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_GNSS.h"
#include "Timezone.h"
#include "GaugePacket.h"

class GNSSModule {
public:
    bool begin();
    void update(GaugePacket &pkt);

    // Raw GNSS data
    sTim_t utc;
    sTim_t date;
    sLonLat_t lat;
    sLonLat_t lon;
    double altitude;
    uint8_t satsUsed;
    double sog;
    double cog;
    uint8_t mode;

    // Timezone conversion
    Timezone tz = Timezone(TZ_EASTERN);
    void getLocalTime(int &h, int &m, int &s,
                      int &mo, int &d, int &y);

    // Helpers
    bool hasFix() const;
    double latitudeDeg() const;
    double longitudeDeg() const;

    // Heading + Compass
    double getHeading() const { return headingDeg; }
    const char* getCompass8() const;

private:
    DFRobot_GNSS_I2C gnss = DFRobot_GNSS_I2C(&Wire, GNSS_DEVICE_ADDR);

    // Heading state
    double lastLat = NAN;
    double lastLon = NAN;
    double headingDeg = NAN;
};
