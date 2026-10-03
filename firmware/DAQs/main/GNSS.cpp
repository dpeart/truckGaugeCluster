#include "GNSS.h"

// ------------------------
// Helpers for bearing math
// ------------------------
static double deg2rad(double d) {
    return d * 0.017453292519943295;
}

static double rad2deg(double r) {
    return r * 57.29577951308232;
}

static double computeBearing(double lat1, double lon1,
                             double lat2, double lon2) {
    double phi1 = deg2rad(lat1);
    double phi2 = deg2rad(lat2);
    double dLon = deg2rad(lon2 - lon1);

    double y = sin(dLon) * cos(phi2);
    double x = cos(phi1) * sin(phi2) -
               sin(phi1) * cos(phi2) * cos(dLon);

    double brng = atan2(y, x);
    brng = rad2deg(brng);
    if (brng < 0) brng += 360.0;

    return brng;
}

// ------------------------
// 8‑point compass mapping
// ------------------------
static const char* headingToCompass8(double headingDeg) {
    if (isnan(headingDeg)) return "UNK";

    while (headingDeg < 0) headingDeg += 360.0;
    while (headingDeg >= 360) headingDeg -= 360.0;

    if (headingDeg >= 337.5 || headingDeg < 22.5) return "N";
    if (headingDeg >= 22.5 && headingDeg < 67.5) return "NE";
    if (headingDeg >= 67.5 && headingDeg < 112.5) return "E";
    if (headingDeg >= 112.5 && headingDeg < 157.5) return "SE";
    if (headingDeg >= 157.5 && headingDeg < 202.5) return "S";
    if (headingDeg >= 202.5 && headingDeg < 247.5) return "SW";
    if (headingDeg >= 247.5 && headingDeg < 292.5) return "W";
    if (headingDeg >= 292.5 && headingDeg < 337.5) return "NW";

    return "UNK";
}

// Public getter for debug printing
const char* GNSSModule::getCompass8() const {
    return headingToCompass8(headingDeg);
}

// ------------------------
// Local time conversion
// ------------------------
void GNSSModule::getLocalTime(int &h, int &m, int &s,
                              int &mo, int &d, int &y) {
    tz.convertUTC(date.year, date.month, date.date,
                  utc.hour, utc.minute, utc.second,
                  h, m, s, mo, d, y);
}

// ------------------------
// GNSS initialization
// ------------------------
bool GNSSModule::begin() {
    Wire.begin();

    while (!gnss.begin()) {
        Serial.println("GNSS: No device found...");
        delay(1000);
    }

    gnss.enablePower();
    gnss.setGnss(eGPS_BeiDou_GLONASS);
    gnss.setRgbOn();

    Serial.println("GNSS: Initialized");
    return true;
}

// ------------------------
// GNSS update + heading
// ------------------------
void GNSSModule::update(GaugePacket &pkt) {

    // Read GNSS hardware
    utc = gnss.getUTC();
    date = gnss.getDate();
    lat = gnss.getLat();
    lon = gnss.getLon();
    altitude = gnss.getAlt();
    satsUsed = gnss.getNumSatUsed();
    sog = gnss.getSog();
    cog = gnss.getCog();
    mode = gnss.getGnssMode();

    // Compute local time
    int th, tm, ts;
    int tmo, td, ty;

    tz.convertUTC(date.year, date.month, date.date,
                  utc.hour, utc.minute, utc.second,
                  th, tm, ts, tmo, td, ty);

    pkt.hour   = th;
    pkt.minute = tm;
    pkt.second = ts;

    pkt.month  = tmo;
    pkt.day    = td;
    pkt.year   = ty;

    // Compute heading (bearing)
    double prevLat = lastLat;
    double prevLon = lastLon;
    double curLat  = lat.latitudeDegree;
    double curLon  = lon.lonitudeDegree;

    if (!isnan(prevLat) && !isnan(prevLon)) {
        double dLat = curLat - prevLat;
        double dLon = curLon - prevLon;

        if (fabs(dLat) > 0.00001 || fabs(dLon) > 0.00001) {
            double newHeading = computeBearing(prevLat, prevLon, curLat, curLon);

            if (isnan(headingDeg))
                headingDeg = newHeading;
            else
                headingDeg = headingDeg * 0.7 + newHeading * 0.3;
        }
    }

    lastLat = curLat;
    lastLon = curLon;

    // Store heading in packet (scaled)
    pkt.headingDeg = (int16_t)(headingDeg * 100.0f);

    // Store compass STRING
    const char* dir = headingToCompass8(headingDeg);
    strncpy(pkt.compass8, dir, sizeof(pkt.compass8));
    pkt.compass8[sizeof(pkt.compass8)-1] = '\0';
}

// ------------------------
// Helpers
// ------------------------
bool GNSSModule::hasFix() const {
    return satsUsed >= 5;  // 5+ = 3D fix
}

double GNSSModule::latitudeDeg() const {
    return lat.latitudeDegree;
}

double GNSSModule::longitudeDeg() const {
    return lon.lonitudeDegree;
}
