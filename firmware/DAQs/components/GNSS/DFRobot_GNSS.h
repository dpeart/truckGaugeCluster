#pragma once
#include <cstdint>
#include <cmath>

// Register map and constants matching original DFRobot library
#define I2C_FLAG          1
#define DFR_GNSS_I2C_ADDR  0x20

#define I2C_YEAR_H        0x00
#define I2C_YEAR_L        0x01
#define I2C_MONTH         0x02
#define I2C_DATE          0x03
#define I2C_HOUR          0x04
#define I2C_MINUTE        0x05
#define I2C_SECOND        0x06

#define I2C_LAT_1         0x07
#define I2C_LAT_2         0x08
#define I2C_LAT_X_24      0x09
#define I2C_LAT_X_16      0x0A
#define I2C_LAT_X_8       0x0B
#define I2C_LON_DIS       0x0C

#define I2C_LON_1         0x0D
#define I2C_LON_2         0x0E
#define I2C_LON_X_24      0x0F
#define I2C_LON_X_16      0x10
#define I2C_LON_X_8       0x11
#define I2C_LAT_DIS       0x12

#define I2C_USE_STAR      0x13
#define I2C_ALT_H         0x14
#define I2C_ALT_L         0x15
#define I2C_ALT_X         0x16

#define I2C_SOG_H         0x17
#define I2C_SOG_L         0x18
#define I2C_SOG_X         0x19

#define I2C_COG_H         0x1A
#define I2C_COG_L         0x1B
#define I2C_COG_X         0x1C

#define I2C_START_GET     0x1D
#define I2C_ID            0x1E
#define I2C_DATA_LEN_H    0x1F
#define I2C_DATA_LEN_L    0x20
#define I2C_ALL_DATA      0x21

#define I2C_GNSS_MODE     0x22
#define I2C_SLEEP_MODE    0x23
#define I2C_RGB_MODE      0x24

#define RGB_ON            0x01
#define RGB_OFF           0x00
#define ENABLE_POWER      0x00
#define DISABLE_POWER     0x01

// Custom Data Types
typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t date;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} sTim_t;

typedef struct {
  uint8_t latDD;
  uint8_t latMM;
  uint32_t latMMMMM;
  double latitude;
  double latitudeDegree;
  char latDirection;

  uint8_t lonDDD;
  uint8_t lonMM;
  uint32_t lonMMMMM;
  double lonitude;
  double lonitudeDegree;
  char lonDirection;
} sLonLat_t;

typedef enum {
  eGPS = 1,
  eBeiDou,
  eGPS_BeiDou,
  eGLONASS,
  eGPS_GLONASS,
  eBeiDou_GLONASS,
  eGPS_BeiDou_GLONASS,
} eGnssMode_t;

// Abstract Base Class
class DFRobot_GNSS {
public:
  uint8_t uartI2CFlag = 0;
  DFRobot_GNSS();
  virtual ~DFRobot_GNSS();

  virtual bool begin() = 0;
  virtual sTim_t getUTC(void) = 0;
  virtual sTim_t getDate(void) = 0;
  virtual sLonLat_t getLat(void) = 0;
  virtual sLonLat_t getLon(void) = 0;
  virtual uint8_t getNumSatUsed(void) = 0;
  virtual double getAlt(void) = 0;
  virtual double getSog(void) = 0;
  virtual double getCog(void) = 0;
  virtual void setGnss(eGnssMode_t mode) = 0;
  virtual uint8_t getGnssMode(void) = 0;
  virtual void getAllGnss(void) = 0;
  virtual void enablePower(void) = 0;
  virtual void disablePower(void) = 0;
  virtual void setRgbOn(void) = 0;
  virtual void setRgbOff(void) = 0;
  virtual void setCallback(void (*call)(char *, uint8_t)) = 0;

protected:
  virtual void writeReg(uint8_t reg, const uint8_t *data, uint8_t len) = 0;
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len) = 0;
};

// I2C Interface Class
class DFRobot_GNSS_I2C : public DFRobot_GNSS {
public:
  DFRobot_GNSS_I2C(uint8_t addr = DFR_GNSS_I2C_ADDR);
  ~DFRobot_GNSS_I2C() override;

  bool begin() override;
  sTim_t getUTC(void) override;
  sTim_t getDate(void) override;
  sLonLat_t getLat(void) override;
  sLonLat_t getLon(void) override;
  uint8_t getNumSatUsed(void) override;
  double getAlt(void) override;
  double getSog(void) override;
  double getCog(void) override;
  void setGnss(eGnssMode_t mode) override;
  uint8_t getGnssMode(void) override;
  void getAllGnss(void) override;
  void enablePower(void) override;
  void disablePower(void) override;
  void setRgbOn(void) override;
  void setRgbOff(void) override;
  void setCallback(void (*call)(char *, uint8_t)) override;

  uint16_t getGnssLen(void);

protected:
  void writeReg(uint8_t reg, const uint8_t *data, uint8_t len) override;
  int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len) override;

private:
  uint8_t _addr;
  void (*callback)(char *, uint8_t) = nullptr;
};