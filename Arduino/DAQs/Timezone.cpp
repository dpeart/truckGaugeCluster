#include "Timezone.h"

// Constructor
Timezone::Timezone(TimezoneRegion r) : region(r) {}

int Timezone::baseOffsetHours() const {
    switch (region) {
        case TZ_EASTERN:  return -5;
        case TZ_CENTRAL:  return -6;
        case TZ_MOUNTAIN: return -7;
        case TZ_PACIFIC:  return -8;
    }
    return -5;
}

// Zeller’s congruence helper for day-of-week
static int dayOfWeek(int y, int m, int d) {
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100;
    int J = y / 100;
    int h = (d + 13*(m+1)/5 + K + K/4 + J/4 + 5*J) % 7;
    return ((h + 6) % 7); // 0 = Sunday
}

// Find the Nth Sunday of a month
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
    int startDay = nthSunday(year, 3, 2);   // 2nd Sunday in March
    int endDay   = nthSunday(year, 11, 1);  // 1st Sunday in November

    // Before DST start
    if (month < 3) return false;
    if (month == 3 && (day < startDay || (day == startDay && hour < 2)))
        return false;

    // After DST end
    if (month > 11) return false;
    if (month == 11 && (day > endDay || (day == endDay && hour >= 2)))
        return false;

    return true;
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
