#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "Timezone.h"
#include "GaugePacket.h"

// Minimal GNSS data types (adjust to your real driver later)
struct GnssUtc
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct GnssDate
{
    uint16_t year;
    uint8_t month;
    uint8_t date;
};

struct GnssLat
{
    double latitudeDegree;
};

struct GnssLon
{
    double lonitudeDegree;
};

// Forward declaration of your GNSS hardware driver.
// Replace this with your real GNSS class when you hook it up.
class GnssHardware
{
public:
    bool begin();
    void enablePower();
    void setGnss(int mode);
    void setRgbOn();

    GnssUtc getUTC();
    GnssDate getDate();
    GnssLat getLat();
    GnssLon getLon();
    double getAlt();
    int getNumSatUsed();
    double getSog();
    double getCog();
    int getGnssMode();
};

// This will be defined in GNSS.cpp
extern GnssHardware gnss;

class GNSSModule
{
public:
    GNSSModule();

    bool begin();
    void update(GaugePacket &pkt);

    void getLocalTime(int &h, int &m, int &s,
                      int &mo, int &d, int &y);

    bool hasFix() const;
    double latitudeDeg() const;
    double longitudeDeg() const;
    const char *getCompass8() const;

private:
    GnssUtc utc;
    GnssDate date;
    GnssLat lat;
    GnssLon lon;

    double altitude = NAN;
    int satsUsed = 0;
    double sog = NAN;
    double cog = NAN;
    int mode = 0;

    double headingDeg = NAN;
    double lastLat = NAN;
    double lastLon = NAN;

    Timezone tz;

    static double deg2rad(double d);
    static double rad2deg(double r);
    static double computeBearing(double lat1, double lon1,
                                 double lat2, double lon2);
    static const char *headingToCompass8(double headingDeg);
};
