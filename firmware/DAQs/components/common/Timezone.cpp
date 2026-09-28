#include "Timezone.h"
#include <cmath>

// Constructor by region
Timezone::Timezone(TimezoneRegion r) : region(r), _lat(NAN), _lon(NAN) {}

// Optional constructor by lat/lon
Timezone::Timezone(double lat, double lon) : _lat(lat), _lon(lon) {
    region = lookupRegion(lat, lon);
}

void Timezone::setLocation(double lat, double lon) {
    _lat = lat;
    _lon = lon;
    region = lookupRegion(lat, lon);
}

// --- existing baseOffsetHours, dayOfWeek, nthSunday, isDST unchanged ---
int Timezone::baseOffsetHours() const {
    switch (region) {
        case TZ_EASTERN:  return -5;
        case TZ_CENTRAL:  return -6;
        case TZ_MOUNTAIN: return -7;
        case TZ_PACIFIC:  return -8;
        case TZ_UTC:      return 0;
    }
    return 0;
}

// Zeller’s congruence helper for day-of-week
static int dayOfWeek(int y, int m, int d) {
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100;
    int J = y / 100;
    int h = (d + 13*(m+1)/5 + K + K/4 + J/4 + 5*J) % 7;
    return ((h + 6) % 7); // 0 = Sunday
}

int Timezone::nthSunday(int year, int month, int n) const {
    int count = 0;
    for (int day = 1; day <= 31; day++) {
        if (dayOfWeek(year, month, day) == 0) {
            count++;
            if (count == n) return day;
        }
    }
    return -1;
}

bool Timezone::isDST(int year, int month, int day, int hour) const {
    // US DST rules (second Sunday March to first Sunday November)
    int startDay = nthSunday(year, 3, 2);   // 2nd Sunday in March
    int endDay   = nthSunday(year, 11, 1);  // 1st Sunday in November

    if (month < 3) return false;
    if (month == 3 && (day < startDay || (day == startDay && hour < 2)))
        return false;

    if (month > 11) return false;
    if (month == 11 && (day > endDay || (day == endDay && hour >= 2)))
        return false;

    return true;
}

// --- New: simple heuristic lookupRegion ---
// This is intentionally small and conservative: it handles the contiguous US reasonably,
// and falls back to UTC for unknown areas. Replace with tzdb polygon lookup for full accuracy.
TimezoneRegion Timezone::lookupRegion(double lat, double lon) const {
    if (std::isnan(lat) || std::isnan(lon)) return TZ_UTC;

    // Quick bounding for contiguous USA (approx)
    if (lat >= 24.0 && lat <= 50.0 && lon <= -66.0 && lon >= -125.0) {
        // Corrected longitude bands (most negative / west on left, least negative / east on right)
        if (lon >= -82.5 && lon < -67.5) return TZ_EASTERN;   // Eastern (-67.5 to -82.5)
        if (lon >= -97.5 && lon < -82.5) return TZ_CENTRAL;  // Central (-82.5 to -97.5)
        if (lon >= -112.5 && lon < -97.5) return TZ_MOUNTAIN;// Mountain (-97.5 to -112.5)
        if (lon <= -112.5 && lon >= -125.0) return TZ_PACIFIC;// Pacific (-112.5 to -125.0)

        return TZ_CENTRAL;
    }
    return TZ_UTC;
}

void Timezone::convertUTC(int year, int month, int day,
                          int hour, int minute, int second,
                          int &outHour, int &outMinute, int &outSecond,
                          int &outMonth, int &outDay, int &outYear) {

    int offset = baseOffsetHours();
    if (isDST(year, month, day, hour)) offset += 1;

    // Apply offset
    hour += offset;

    // Normalize
    outYear = year;
    outMonth = month;
    outDay = day;
    outHour = hour;
    outMinute = minute;
    outSecond = second;

    while (outHour < 0) {
        outHour += 24;
        outDay--;
    }
    while (outHour >= 24) {
        outHour -= 24;
        outDay++;
    }

    // Month/day rollover (simple version)
    static const int daysInMonth[] =
        {0,31,28,31,30,31,30,31,31,30,31,30,31};

    int dim = daysInMonth[outMonth];
    if (outMonth == 2 && (outYear % 4 == 0)) dim = 29;

    if (outDay > dim) {
        outDay = 1;
        outMonth++;
        if (outMonth > 12) { outMonth = 1; outYear++; }
    }
    if (outDay < 1) {
        outMonth--;
        if (outMonth < 1) { outMonth = 12; outYear--; }
        int dimPrev = daysInMonth[outMonth];
        if (outMonth == 2 && (outYear % 4 == 0)) dimPrev = 29;
        outDay = dimPrev;
    }
}
