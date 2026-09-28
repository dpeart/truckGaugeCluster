#include "GNSS.h"
#include <cmath>
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "DFRobot_GNSS.h"

static const char *TAG = "GNSS";

// ------------------------
// Helpers for bearing math (class static definitions)
// ------------------------
double GNSSModule::deg2rad(double d)
{
    return d * 0.017453292519943295;
}

double GNSSModule::rad2deg(double r)
{
    return r * 57.29577951308232;
}

double GNSSModule::computeBearing(double lat1, double lon1,
                                  double lat2, double lon2)
{
    double phi1 = deg2rad(lat1);
    double phi2 = deg2rad(lat2);
    double dLon = deg2rad(lon2 - lon1);

    double y = sin(dLon) * cos(phi2);
    double x = cos(phi1) * sin(phi2) -
               sin(phi1) * cos(phi2) * cos(dLon);

    double brng = atan2(y, x);
    brng = rad2deg(brng);
    if (brng < 0)
        brng += 360.0;

    return brng;
}

// ------------------------
// 8‑point compass mapping (class static)
// ------------------------
const char *GNSSModule::headingToCompass8(double headingDeg)
{
    if (isnan(headingDeg))
        return "UNK";

    while (headingDeg < 0)
        headingDeg += 360.0;
    while (headingDeg >= 360)
        headingDeg -= 360.0;

    if (headingDeg >= 337.5 || headingDeg < 22.5)
        return "N";
    if (headingDeg >= 22.5 && headingDeg < 67.5)
        return "NE";
    if (headingDeg >= 67.5 && headingDeg < 112.5)
        return "E";
    if (headingDeg >= 112.5 && headingDeg < 157.5)
        return "SE";
    if (headingDeg >= 157.5 && headingDeg < 202.5)
        return "S";
    if (headingDeg >= 202.5 && headingDeg < 247.5)
        return "SW";
    if (headingDeg >= 247.5 && headingDeg < 292.5)
        return "W";
    if (headingDeg >= 292.5 && headingDeg < 337.5)
        return "NW";

    return "UNK";
}

// Public getter for debug printing
const char *GNSSModule::getCompass8() const
{
    return headingToCompass8(headingDeg);
}

// ------------------------
// Local time conversion (declared in header)
// ------------------------
void GNSSModule::getLocalTime(int &h, int &m, int &s,
                              int &mo, int &d, int &y)
{
    tz.convertUTC(date.year, date.month, date.date,
                  utc.hour, utc.minute, utc.second,
                  h, m, s, mo, d, y);
}

// ------------------------
// GNSS initialization
// ------------------------
// IMPORTANT: initializer order must match declaration order in GNSS.h
GNSSModule::GNSSModule()
    : altitude(NAN) // declared before satsUsed in header
      ,
      satsUsed(0), sog(NAN), cog(NAN), mode(0), headingDeg(NAN), lastLat(NAN), lastLon(NAN), tz(TZ_UTC) // tz is declared after lastLon in header
{
}
bool GNSSModule::begin()
{
    // Retry loop until the coprocessor responds over I2C
    while (!gnss.begin())
    {
        ESP_LOGW(TAG, "GNSS: No device found, retrying...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Give the coprocessor firmware time to fully boot up after I2C connection
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Configure GNSS options once stable
    gnss.enablePower();
    vTaskDelay(pdMS_TO_TICKS(100)); // Small buffer between commands
    gnss.setGnss(eGPS_BeiDou_GLONASS);
    vTaskDelay(pdMS_TO_TICKS(100));
    gnss.setRgbOn();

    ESP_LOGI(TAG, "GNSS: Initialized");
    return true;
}

// ------------------------
// GNSS update + heading
// ------------------------
void GNSSModule::update(GaugePacket &pkt)
{
    // HARD RATE LIMIT: Only execute I2C reads once every 1000 ms (1 second) using FreeRTOS ticks
    static TickType_t lastI2cUpdate = 0;
    TickType_t now = xTaskGetTickCount();

    // pdMS_TO_TICKS(1000) converts 1 second into FreeRTOS ticks
    if ((now - lastI2cUpdate) < pdMS_TO_TICKS(1000))
    {
        // Return immediately if called faster than 1 Hz.
        return;
    }
    lastI2cUpdate = now;

    // 1. Fetch raw readings into matching struct types
    GnssUtc newUtc = gnss.getUTC();
    GnssDate newDate = gnss.getDate();
    GnssLat newLat = gnss.getLat();
    GnssLon newLon = gnss.getLon();
    double newAlt = gnss.getAlt();
    int newSats = gnss.getNumSatUsed();
    double newSog = gnss.getSog();
    double newCog = gnss.getCog();
    auto newMode = gnss.getGnssMode();

    // 2. Validate & preserve state on I2C read failures
    if (newDate.year >= 2024)
    {
        date = newDate;
    }

    if (newUtc.hour != 0 || newUtc.minute != 0 || newUtc.second != 0)
    {
        utc = newUtc;
    }

    double curLat = newLat.latitudeDegree;
    double curLon = newLon.lonitudeDegree;

    // Filter out NaN and Null Island (0.0, 0.0) false positives
    bool hasValidPos = !isnan(curLat) && !isnan(curLon) &&
                       (fabs(curLat) > 0.001 || fabs(curLon) > 0.001);

    if (hasValidPos)
    {
        lat = newLat;
        lon = newLon;
    }

    if (!isnan(newAlt))
        altitude = newAlt;
    if (newSats >= 0)
        satsUsed = newSats;
    if (!isnan(newSog))
        sog = newSog;
    if (!isnan(newCog))
        cog = newCog;
    mode = newMode;

    // 3. Update timezone location only with valid coordinates
    if (hasValidPos)
    {
        static double lastTzLat = NAN;
        static double lastTzLon = NAN;
        const double tzChangeThresholdDeg = 0.5;

        bool needUpdate = false;
        if (isnan(lastTzLat) || isnan(lastTzLon))
        {
            needUpdate = true;
        }
        else
        {
            double dLat = fabs(curLat - lastTzLat);
            double dLon = fabs(curLon - lastTzLon);
            if (dLat > tzChangeThresholdDeg || dLon > tzChangeThresholdDeg)
                needUpdate = true;
        }

        if (needUpdate)
        {
            tz.setLocation(curLat, curLon);
            lastTzLat = curLat;
            lastTzLon = curLon;
            ESP_LOGI(TAG, "Timezone updated from lat=%.6f lon=%.6f", curLat, curLon);
        }
    }

    // 4. Compute local time (only when date structure contains valid data)
    if (date.year >= 2024)
    {
        int th = 0, tm = 0, ts = 0;
        int tmo = 0, td = 0, ty = 0;

        tz.convertUTC(date.year, date.month, date.date,
                      utc.hour, utc.minute, utc.second,
                      th, tm, ts, tmo, td, ty);

        pkt.hour = th;
        pkt.minute = tm;
        pkt.second = ts;

        pkt.month = tmo;
        pkt.day = td;
        pkt.year = ty;
    }

    // 5. Compute heading / bearing using valid positional movements
    if (hasValidPos)
    {
        double prevLat = lastLat;
        double prevLon = lastLon;

        if (!isnan(prevLat) && !isnan(prevLon) &&
            (fabs(prevLat) > 0.001 || fabs(prevLon) > 0.001))
        {
            double dLat = curLat - prevLat;
            double dLon = curLon - prevLon;

            if (fabs(dLat) > 0.00001 || fabs(dLon) > 0.00001)
            {
                double newHeading = computeBearing(prevLat, prevLon, curLat, curLon);

                if (isnan(headingDeg))
                    headingDeg = newHeading;
                else
                    headingDeg = headingDeg * 0.7 + newHeading * 0.3;
            }
        }

        lastLat = curLat;
        lastLon = curLon;
    }

    // Store heading in packet
    pkt.headingDeg = (int16_t)(headingDeg * 100.0f);

    // Store compass string
    const char *dir = headingToCompass8(headingDeg);
    strncpy(pkt.compass8, dir, sizeof(pkt.compass8));
    pkt.compass8[sizeof(pkt.compass8) - 1] = '\0';

    // 6. Diagnostic Log Output (1 Hz)
    ESP_LOGI(TAG, "Sats: %d | UTC: %02d:%02d:%02d | Local: %02d:%02d:%02d | Lat: %.6f | Lon: %.6f",
             satsUsed, utc.hour, utc.minute, utc.second,
             pkt.hour, pkt.minute, pkt.second,
             lat.latitudeDegree, lon.lonitudeDegree);
}
// ------------------------
// Helpers
// ------------------------
bool GNSSModule::hasFix() const
{
    return satsUsed >= 5; // 5+ = 3D fix
}

double GNSSModule::latitudeDeg() const
{
    return lat.latitudeDegree;
}

double GNSSModule::longitudeDeg() const
{
    return lon.lonitudeDegree;
}
