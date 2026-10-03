#include "DFRobot_GNSS.h"
#include "esp_i2c_helpers.h"

DFRobot_GNSS::DFRobot_GNSS() {}
DFRobot_GNSS::~DFRobot_GNSS() {}

static inline void delayMs(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

sTim_t DFRobot_GNSS::getDate(void)
{
  sTim_t data = {0};
  uint8_t _sendData[10] = {0};
  readReg(I2C_YEAR_H, _sendData, 4);
  data.year = ((uint16_t)_sendData[0] << 8) | _sendData[1];
  data.month = _sendData[2];
  data.date = _sendData[3];
  return data;
}

sTim_t DFRobot_GNSS::getUTC(void)
{
  sTim_t data = {0};
  uint8_t _sendData[10] = {0};
  readReg(I2C_HOUR, _sendData, 3);
  data.hour   = _sendData[0];
  data.minute = _sendData[1];
  data.second = _sendData[2];
  return data;
}

sLonLat_t DFRobot_GNSS::getLat(void)
{
  sLonLat_t data = {0};
  uint8_t _sendData[10] = {0};
  readReg(I2C_LAT_1, _sendData, 6);
  data.latDD  = _sendData[0];
  data.latMM = _sendData[1];
  data.latMMMMM = ((uint32_t)_sendData[2] << 16) | ((uint32_t)_sendData[3] << 8) | ((uint32_t)_sendData[4]);
  data.latitude = (double)data.latDD * 100.0 + ((double)data.latMM) + ((double)data.latMMMMM / 100000.0);
  data.latitudeDegree = (double)data.latDD + (double)data.latMM / 60.0 + (double)data.latMMMMM / 100000.0 / 60.0;

  readReg(I2C_LAT_DIS, _sendData, 1);
  data.latDirection = (char)_sendData[0];
  return data;
}

sLonLat_t DFRobot_GNSS::getLon(void)
{
  sLonLat_t data = {0};
  uint8_t _sendData[10] = {0};
  readReg(I2C_LON_1, _sendData, 6);
  data.lonDDD  = _sendData[0];
  data.lonMM = _sendData[1];
  data.lonMMMMM = ((uint32_t)_sendData[2] << 16) | ((uint32_t)_sendData[3] << 8) | ((uint32_t)_sendData[4]);
  data.lonitude = (double)data.lonDDD * 100.0 + ((double)data.lonMM) + ((double)data.lonMMMMM / 100000.0);
  data.lonitudeDegree = (double)data.lonDDD + (double)data.lonMM / 60.0 + (double)data.lonMMMMM / 100000.0 / 60.0;

  readReg(I2C_LON_DIS, _sendData, 1);
  data.lonDirection = (char)_sendData[0];
  return data;
}

uint8_t DFRobot_GNSS::getNumSatUsed(void)
{
  uint8_t _sendData[10] = {0};
  readReg(I2C_USE_STAR, _sendData, 1);
  return _sendData[0];
}

double DFRobot_GNSS::getAlt(void)
{
  double high;
  uint8_t _sendData[10] = {0};
  readReg(I2C_ALT_H, _sendData, 3);
  high = (double)((uint16_t)(_sendData[0] & 0x7F) << 8 | _sendData[1]) + (double)_sendData[2] / 100.0;
  return high;
}

double DFRobot_GNSS::getSog(void)
{
  double speed;
  uint8_t _sendData[10] = {0};
  readReg(I2C_SOG_H, _sendData, 3);
  speed = (double)((uint16_t)(_sendData[0] & 0x7F) << 8 | _sendData[1]) + (double)_sendData[2] / 100.0;
  return speed;
}

double DFRobot_GNSS::getCog(void)
{
  double cog;
  uint8_t _sendData[10] = {0};
  readReg(I2C_COG_H, _sendData, 3);
  cog = (double)((uint16_t)(_sendData[0] & 0x7F) << 8 | _sendData[1]) + (double)_sendData[2] / 100.0;
  return cog;
}

void DFRobot_GNSS::setRgbOn(void)
{
  uint8_t _sendData[1] = {RGB_ON};
  writeReg(I2C_RGB_MODE, _sendData, 1);
  delayMs(50);
}

void DFRobot_GNSS::setRgbOff(void)
{
  uint8_t _sendData[1] = {RGB_OFF};
  writeReg(I2C_RGB_MODE, _sendData, 1);
  delayMs(50);
}

void DFRobot_GNSS::enablePower(void)
{
  uint8_t _sendData[1] = {ENABLE_POWER};
  writeReg(I2C_SLEEP_MODE, _sendData, 1);
  delayMs(50);
}

void DFRobot_GNSS::disablePower(void)
{
  uint8_t _sendData[1] = {DISABLE_POWER};
  writeReg(I2C_SLEEP_MODE, _sendData, 1);
  delayMs(50);
}

void DFRobot_GNSS::setGnss(eGnssMode_t mode)
{
  uint8_t _sendData[1] = {static_cast<uint8_t>(mode)};
  writeReg(I2C_GNSS_MODE, _sendData, 1);
  delayMs(50);
}

uint8_t DFRobot_GNSS::getGnssMode(void)
{
  uint8_t _sendData[10] = {0};
  readReg(I2C_GNSS_MODE, _sendData, 1);
  return _sendData[0];
}

uint16_t DFRobot_GNSS::getGnssLen(void)
{
  uint8_t _sendData[10] = {0};
  _sendData[0] = 0x55;                  // enable measure
  writeReg(I2C_START_GET, _sendData, 1);
  delayMs(100);
  readReg(I2C_DATA_LEN_H, _sendData, 2);
  return (uint16_t)_sendData[0] << 8 | _sendData[1];
}

void DFRobot_GNSS::getAllGnss(void)
{
  uint8_t _sendData[260] = {0};
  uint16_t len = getGnssLen();
  if (len > 1024 + 200 || len == 0) {
    return;
  }

  // Pure I2C block size of 32 bytes
  uint8_t templen = len / 32 + 1;
  for (uint16_t i = 0; i < templen; i++) {
    uint8_t chunk_size = (i == templen - 1) ? (len % 32) : 32;
    if (chunk_size == 0) continue;

    readReg(I2C_ALL_DATA, _sendData, chunk_size);
    for (uint8_t j = 0; j < chunk_size; j++) {
      if (_sendData[j] == '\0') {
        _sendData[j] = '\n';
      }
    }

    if (callback) {
      callback((char *)_sendData, chunk_size);
    }
  }
}

void DFRobot_GNSS::setCallback(void (*call)(char *, uint8_t))
{
  this->callback = call;
}

/* Concrete I2C Implementation */

DFRobot_GNSS_I2C::DFRobot_GNSS_I2C(i2c_port_t i2c_port, uint8_t addr)
  : _i2c_port(i2c_port), _I2C_addr(addr)
{
}

bool DFRobot_GNSS_I2C::begin()
{
  uint8_t dummy = 0;
  // Test communication by attempting to read register I2C_ID
  return (readReg(I2C_ID, &dummy, 1) == 0);
}

void DFRobot_GNSS_I2C::writeReg(uint8_t reg, uint8_t *data, uint8_t len)
{
  esp_i2c_write_reg(_i2c_port, _I2C_addr, reg, data, len);
}

int16_t DFRobot_GNSS_I2C::readReg(uint8_t reg, uint8_t *data, uint8_t len)
{
  esp_err_t ret = esp_i2c_read_reg(_i2c_port, _I2C_addr, reg, data, len);
  return (ret == ESP_OK) ? 0 : -1;
}