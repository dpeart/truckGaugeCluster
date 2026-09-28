// components/DFRobot_GNSS/GnssHardwareAdapter.cpp
#include "GNSS.h"                    // your existing GNSS.h (declares GnssHardware)
#include "DFRobot_GNSS.h"        // converted DFRobot header

// Create a single driver instance (I2C)
static DFRobot_GNSS_I2C dfDriver; // default address 0x20

// Provide the global symbol expected by GNSS.h
GnssHardware gnss;

// Forwarding implementations: call into dfDriver
bool GnssHardware::begin() { return dfDriver.begin(); }
void GnssHardware::enablePower() { dfDriver.enablePower(); }
void GnssHardware::setGnss(int mode) { dfDriver.setGnss((eGnssMode_t)mode); }
void GnssHardware::setRgbOn() { dfDriver.setRgbOn(); }

GnssUtc GnssHardware::getUTC() { sTim_t t = dfDriver.getUTC(); GnssUtc u = {t.hour, t.minute, t.second}; return u; }
GnssDate GnssHardware::getDate() { sTim_t t = dfDriver.getDate(); GnssDate d = {t.year, t.month, t.date}; return d; }
GnssLat  GnssHardware::getLat()  { sLonLat_t s = dfDriver.getLat(); GnssLat g; g.latitudeDegree = s.latitudeDegree; return g; }
GnssLon  GnssHardware::getLon()  { sLonLat_t s = dfDriver.getLon(); GnssLon g; g.lonitudeDegree = s.lonitudeDegree; return g; }
double   GnssHardware::getAlt()  { return dfDriver.getAlt(); }
int      GnssHardware::getNumSatUsed() { return (int)dfDriver.getNumSatUsed(); }
double   GnssHardware::getSog()  { return dfDriver.getSog(); }
double   GnssHardware::getCog()  { return dfDriver.getCog(); }
int      GnssHardware::getGnssMode() { return (int)dfDriver.getGnssMode(); }
