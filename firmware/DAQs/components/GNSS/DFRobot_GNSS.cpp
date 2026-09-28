#include "DFRobot_GNSS.h"
#include "sequent_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cmath>

static const char *TAG = "DFRobot_GNSS";

// Base class constructor/destructor
DFRobot_GNSS::DFRobot_GNSS() { uartI2CFlag = 0; }
DFRobot_GNSS::~DFRobot_GNSS() {}

// DFRobot_GNSS_I2C implementation
DFRobot_GNSS_I2C::DFRobot_GNSS_I2C(uint8_t addr) : _addr(addr)
{
  uartI2CFlag = I2C_FLAG;
}

DFRobot_GNSS_I2C::~DFRobot_GNSS_I2C() {}

bool DFRobot_GNSS_I2C::begin()
{
  uint8_t test_val = 0;
  // Verify device presence by checking register 0x00 (I2C_YEAR_H)
  if (readReg(I2C_YEAR_H, &test_val, 1) == 0)
  {
    ESP_LOGI(TAG, "DFRobot GNSS initialized successfully on address 0x%02X", _addr);
    return true;
  }
  ESP_LOGW(TAG, "DFRobot GNSS not responding on address 0x%02X", _addr);
  return false;
}

void DFRobot_GNSS_I2C::writeReg(uint8_t reg, const uint8_t *data, uint8_t len)
{
  sequent_i2c_write(_addr, reg, data, len);
}

int16_t DFRobot_GNSS_I2C::readReg(uint8_t reg, uint8_t *data, uint8_t len)
{
  return sequent_i2c_read(_addr, reg, data, len);
}

sTim_t DFRobot_GNSS_I2C::getDate(void)
{
  sTim_t data;
  uint8_t buf[4] = {0};
  if (readReg(I2C_YEAR_H, buf, 4) == 0)
  {
    data.year = ((uint16_t)buf[0] << 8) | buf[1];
    data.month = buf[2];
    data.date = buf[3];
  }
  else
  {
    data.year = 1970;
    data.month = 1;
    data.date = 1;
  }
  return data;
}

sTim_t DFRobot_GNSS_I2C::getUTC(void)
{
  sTim_t data = {0, 0, 0, 0, 0, 0};
  uint8_t buf[3] = {0};
  if (readReg(I2C_HOUR, buf, 3) == 0)
  {
    data.hour = buf[0];
    data.minute = buf[1];
    data.second = buf[2];
  }
  return data;
}

sLonLat_t DFRobot_GNSS_I2C::getLat(void)
{
  sLonLat_t data;
  memset(&data, 0, sizeof(data));
  uint8_t buf[6] = {0};

  if (readReg(I2C_LAT_1, buf, 6) == 0)
  {
    data.latDD = buf[0];
    data.latMM = buf[1];
    data.latMMMMM = ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 8) | buf[4];
    data.latitude = (double)data.latDD * 100.0 + ((double)data.latMM) + ((double)data.latMMMMM / 100000.0);
    data.latitudeDegree = (double)data.latDD + (double)data.latMM / 60.0 + (double)data.latMMMMM / 100000.0 / 60.0;

    uint8_t dir = 0;
    readReg(I2C_LAT_DIS, &dir, 1);
    data.latDirection = (char)dir;

    // Apply sign based on direction
    if (data.latDirection == 'S')
    {
      data.latitudeDegree = -data.latitudeDegree;
    }
  }
  else
  {
    data.latitudeDegree = NAN;
  }
  return data;
}

sLonLat_t DFRobot_GNSS_I2C::getLon(void)
{
  sLonLat_t data;
  memset(&data, 0, sizeof(data));
  uint8_t buf[6] = {0};
  if (readReg(I2C_LON_1, buf, 6) == 0)
  {
    data.lonDDD = buf[0];
    data.lonMM = buf[1];
    data.lonMMMMM = ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 8) | buf[4];
    data.lonitude = (double)data.lonDDD * 100.0 + ((double)data.lonMM) + ((double)data.lonMMMMM / 100000.0);
    data.lonitudeDegree = (double)data.lonDDD + (double)data.lonMM / 60.0 + (double)data.lonMMMMM / 100000.0 / 60.0;
    uint8_t dir = 0;
    readReg(I2C_LON_DIS, &dir, 1);
    data.lonDirection = (char)dir;

    // Apply sign based on direction
    if (data.lonDirection == 'W')
    {
      data.lonitudeDegree = -data.lonitudeDegree;
    }
  }
  else
  {
    data.lonitudeDegree = NAN;
  }
  return data;
}

uint8_t DFRobot_GNSS_I2C::getNumSatUsed(void)
{
  uint8_t v = 0;
  if (readReg(I2C_USE_STAR, &v, 1) == 0)
    return v;
  return 0;
}

double DFRobot_GNSS_I2C::getAlt(void)
{
  uint8_t buf[3] = {0};
  if (readReg(I2C_ALT_H, buf, 3) == 0)
  {
    double high = (double)(((uint16_t)(buf[0] & 0x7F) << 8) | buf[1]) + (double)buf[2] / 100.0;
    return high;
  }
  return NAN;
}

double DFRobot_GNSS_I2C::getSog(void)
{
  uint8_t buf[3] = {0};
  if (readReg(I2C_SOG_H, buf, 3) == 0)
  {
    double speed = (double)(((uint16_t)(buf[0] & 0x7F) << 8) | buf[1]) + (double)buf[2] / 100.0;
    return speed;
  }
  return NAN;
}

double DFRobot_GNSS_I2C::getCog(void)
{
  uint8_t buf[3] = {0};
  if (readReg(I2C_COG_H, buf, 3) == 0)
  {
    double cog = (double)(((uint16_t)(buf[0] & 0x7F) << 8) | buf[1]) + (double)buf[2] / 100.0;
    return cog;
  }
  return NAN;
}

void DFRobot_GNSS_I2C::setRgbOn(void)
{
  uint8_t b = RGB_ON;
  writeReg(I2C_RGB_MODE, &b, 1);
  vTaskDelay(pdMS_TO_TICKS(50));
}

void DFRobot_GNSS_I2C::setRgbOff(void)
{
  uint8_t b = RGB_OFF;
  writeReg(I2C_RGB_MODE, &b, 1);
  vTaskDelay(pdMS_TO_TICKS(50));
}

void DFRobot_GNSS_I2C::enablePower(void)
{
  uint8_t b = ENABLE_POWER;
  writeReg(I2C_SLEEP_MODE, &b, 1);
  vTaskDelay(pdMS_TO_TICKS(50));
}

void DFRobot_GNSS_I2C::disablePower(void)
{
  uint8_t b = DISABLE_POWER;
  writeReg(I2C_SLEEP_MODE, &b, 1);
  vTaskDelay(pdMS_TO_TICKS(50));
}

void DFRobot_GNSS_I2C::setGnss(eGnssMode_t mode)
{
  uint8_t b = (uint8_t)mode;
  writeReg(I2C_GNSS_MODE, &b, 1);
  vTaskDelay(pdMS_TO_TICKS(50));
}

uint8_t DFRobot_GNSS_I2C::getGnssMode(void)
{
  uint8_t v = 0;
  if (readReg(I2C_GNSS_MODE, &v, 1) == 0)
    return v;
  return 0;
}

uint16_t DFRobot_GNSS_I2C::getGnssLen(void)
{
  uint8_t buf[2] = {0};
  uint8_t b = 0x55;
  writeReg(I2C_START_GET, &b, 1);
  vTaskDelay(pdMS_TO_TICKS(100));
  if (readReg(I2C_DATA_LEN_H, buf, 2) == 0)
  {
    return (uint16_t)buf[0] << 8 | buf[1];
  }
  return 0;
}

void DFRobot_GNSS_I2C::getAllGnss(void)
{
  uint16_t len = getGnssLen();
  if (len == 0 || len > 2048)
    return;
  uint8_t chunk[260];
  uint16_t remaining = len;
  while (remaining)
  {
    uint8_t toRead = (remaining > 32) ? 32 : (uint8_t)remaining;
    if (readReg(I2C_ALL_DATA, chunk, toRead) == 0)
    {
      // Data chunk processing
    }
    remaining -= toRead;
  }
}

void DFRobot_GNSS_I2C::setCallback(void (*call)(char *, uint8_t))
{
  callback = call;
}