#pragma once

enum TimezoneRegion {
    TZ_UTC = 0,
    TZ_EASTERN,
    TZ_CENTRAL,
    TZ_MOUNTAIN,
    TZ_PACIFIC,
    // add more regions if you implement full tzdb
};

class Timezone {
public:
    Timezone(TimezoneRegion r);
    // New: construct from lat/lon (optional)
    Timezone(double lat, double lon);

    // New API: set current location (lat, lon in degrees)
    void setLocation(double lat, double lon);

    // Convert UTC to local time using current region (or region set explicitly)
    void convertUTC(int year, int month, int day,
                    int hour, int minute, int second,
                    int &outHour, int &outMinute, int &outSecond,
                    int &outMonth, int &outDay, int &outYear);

    // Existing helpers
    int baseOffsetHours() const;
    int nthSunday(int year, int month, int n) const;
    bool isDST(int year, int month, int day, int hour) const;

private:
    TimezoneRegion region;
    double _lat;
    double _lon;

    // New: determine region from lat/lon (stub/heuristic or tzdb)
    TimezoneRegion lookupRegion(double lat, double lon) const;
};
