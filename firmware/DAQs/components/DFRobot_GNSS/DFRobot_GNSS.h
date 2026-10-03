#ifndef __DFROBOT_GNSS_H__
#define __DFROBOT_GNSS_H__

#include <cstdint>
#include <cstddef>
#include "esp_i2c_helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @struct sTim_t
 * @brief Store the time and date information obtained from GPS 
 */
typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t date;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} sTim_t;

/**
 * @struct sLonLat_t
 * @brief Store latitude, longitude and direction information obtained from GPS 
 */
typedef struct {
  uint8_t lonDDD;
  uint8_t lonMM;
  uint32_t lonMMMMM;
  char lonDirection;
  uint8_t latDD;
  uint8_t latMM;
  uint32_t latMMMMM;
  char latDirection;
  double latitude;
  double latitudeDegree;
  double lonitude;
  double lonitudeDegree;
} sLonLat_t;

/**
 * @brief Set GNSS mode
 */
typedef enum {
  eGPS = 1,
  eBeiDou,
  eGPS_BeiDou,
  eGLONASS,
  eGPS_GLONASS,
  eBeiDou_GLONASS,
  eGPS_BeiDou_GLONASS,
} eGnssMode_t;

class DFRobot_GNSS {
public:
  static constexpr uint8_t GNSS_DEVICE_ADDR = 0x20;

  static constexpr uint8_t I2C_YEAR_H      = 0;
  static constexpr uint8_t I2C_YEAR_L      = 1;
  static constexpr uint8_t I2C_MONTH       = 2;
  static constexpr uint8_t I2C_DATE        = 3;
  static constexpr uint8_t I2C_HOUR        = 4;
  static constexpr uint8_t I2C_MINUTE      = 5;
  static constexpr uint8_t I2C_SECOND      = 6;
  static constexpr uint8_t I2C_LAT_1       = 7;
  static constexpr uint8_t I2C_LAT_2       = 8;
  static constexpr uint8_t I2C_LAT_X_24    = 9;
  static constexpr uint8_t I2C_LAT_X_16    = 10;
  static constexpr uint8_t I2C_LAT_X_8     = 11;
  static constexpr uint8_t I2C_LON_DIS     = 12;
  static constexpr uint8_t I2C_LON_1       = 13;
  static constexpr uint8_t I2C_LON_2       = 14;
  static constexpr uint8_t I2C_LON_X_24    = 15;
  static constexpr uint8_t I2C_LON_X_16    = 16;
  static constexpr uint8_t I2C_LON_X_8     = 17;
  static constexpr uint8_t I2C_LAT_DIS     = 18;
  static constexpr uint8_t I2C_USE_STAR    = 19;
  static constexpr uint8_t I2C_ALT_H       = 20;
  static constexpr uint8_t I2C_ALT_L       = 21;
  static constexpr uint8_t I2C_ALT_X       = 22;

  static constexpr uint8_t I2C_SOG_H       = 23;
  static constexpr uint8_t I2C_SOG_L       = 24;
  static constexpr uint8_t I2C_SOG_X       = 25;
  static constexpr uint8_t I2C_COG_H       = 26;
  static constexpr uint8_t I2C_COG_L       = 27;
  static constexpr uint8_t I2C_COG_X       = 28;

  static constexpr uint8_t I2C_START_GET   = 29;
  static constexpr uint8_t I2C_ID          = 30;
  static constexpr uint8_t I2C_DATA_LEN_H  = 31;
  static constexpr uint8_t I2C_DATA_LEN_L  = 32;
  static constexpr uint8_t I2C_ALL_DATA    = 33;

  static constexpr uint8_t I2C_GNSS_MODE   = 34;
  static constexpr uint8_t I2C_SLEEP_MODE  = 35;
  static constexpr uint8_t I2C_RGB_MODE    = 36;

  static constexpr uint8_t ENABLE_POWER   = 0;
  static constexpr uint8_t DISABLE_POWER  = 1;
  
  static constexpr uint8_t RGB_ON         = 0x05;
  static constexpr uint8_t RGB_OFF        = 0x02;

  DFRobot_GNSS();
  virtual ~DFRobot_GNSS();

  sTim_t getUTC(void);
  sTim_t getDate(void);
  sLonLat_t getLat(void);
  sLonLat_t getLon(void);
  uint8_t getNumSatUsed(void);
  double getAlt(void);
  double getSog(void);
  double getCog(void);

  void setGnss(eGnssMode_t mode);
  uint8_t getGnssMode(void);
  void getAllGnss(void);
  void enablePower(void);
  void disablePower(void);
  void setRgbOn(void);
  void setRgbOff(void);

  void setCallback(void (*call)(char *, uint8_t));

  void (*callback)(char *data, uint8_t len) = nullptr;

private:
  uint16_t getGnssLen(void);
  virtual void writeReg(uint8_t reg, uint8_t *data, uint8_t len) = 0;
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len) = 0;
};

class DFRobot_GNSS_I2C : public DFRobot_GNSS {
public:
  DFRobot_GNSS_I2C(i2c_port_t i2c_port = I2C_NUM_0, uint8_t addr = 0x75);
  bool begin(void);

protected:
  virtual void writeReg(uint8_t reg, uint8_t *data, uint8_t len) override;
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len) override;

private:
  i2c_port_t _i2c_port;
  uint8_t _I2C_addr;
};

#endif // __DFROBOT_GNSS_H__