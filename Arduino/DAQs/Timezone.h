#pragma once
#include <Arduino.h>

enum TimezoneRegion {
    TZ_EASTERN,
    TZ_CENTRAL,
    TZ_MOUNTAIN,
    TZ_PACIFIC
};

class Timezone {
public:
    Timezone(TimezoneRegion region);

    // Convert UTC → local time with DST
    void convertUTC(int year, int month, int day,
                    int hour, int minute, int second,
                    int &outHour, int &outMinute, int &outSecond,
                    int &outMonth, int &outDay, int &outYear);

private:
    TimezoneRegion region;
    int baseOffsetHours() const;
    bool isDST(int year, int month, int day, int hour) const;

    int nthSunday(int year, int month, int n) const;
};
