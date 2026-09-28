#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <driver/gpio.h>

// ---------------------------------------------------------
// Feature flags
// ---------------------------------------------------------
#define DEBUG
#define X4

// ---------------------------------------------------------
// I2C Addresses
// ---------------------------------------------------------
#define I2C_ACCEL_ADR      0x68
#define I2C_EGT_ADR        0x67
#define I2C_ODOMETER_ADR   0x50
#define ADC_CARD           0x58
#define DIG_CARD           0x5A
#define RTD_CARD           0x5C
#define I2C_BATTERY_ADR    0x48

// ---------------------------------------------------------
// Digital Input Card Channels
// ---------------------------------------------------------
#define DIG_OVER_DRIVE     1
#define DIG_TCC            2
#define DIG_LEFT           3
#define DIG_RIGHT          4
#define DIG_BRAKE          5
#define DIG_HEAD_LOW       6
#define DIG_HEAD_HIGH      7
#define DIG_RUNNING        8
#define DIG_WATER_FUEL     9
#define DIG_LOW_WASHER     10
#define DIG_CRUISE_ON      11
#define DIG_CRUISE_SET     12
#define DIG_CRUISE_RESUME  13
#define DIG_BRAKE_LIGHT    14
#define DIG_IGNITION       15

// ---------------------------------------------------------
// ESP32 GPIO Pins (converted from Arduino macros)
// ---------------------------------------------------------
static const gpio_num_t PWM_SPEED = GPIO_NUM_13;
static const gpio_num_t PWM_TACH  = GPIO_NUM_19;

static const gpio_num_t SHUTDOWN  = GPIO_NUM_14;

// ---------------------------------------------------------
// ADC Card Channels
// ---------------------------------------------------------
#define ADC_GEAR           2
#define ADC_TPS            3
#define ADC_BOOST_PRESSURE 10
#define ADC_OIL_PRESSURE   12
#define ADC_FUEL_PRESSURE  14
#define ADC_FUEL_LEVEL     4

// ---------------------------------------------------------
// RTD Card Channels
// ---------------------------------------------------------
#define RTD_IA_TEMP        1
#define RTD_OIL_TEMP       2
#define RTD_TRANS_TEMP     3
#define RTD_COOLANT_TEMP   4
#define RTD_AMBIENT_TEMP   5

// ---------------------------------------------------------
// Scaling
// ---------------------------------------------------------
#define INT_SCALING 100

// ---------------------------------------------------------
// External variables (defined in main or DAQ modules)
// ---------------------------------------------------------
extern int dig[];

extern unsigned int cruiseAccel;
extern unsigned int cruiseActive;
extern unsigned int cruiseDecel;
extern unsigned int cruiseSetValue;
extern unsigned int cruiseSpeedActive;

extern unsigned int digitalPins;

// Odometer
extern uint32_t odometer;
extern uint32_t accumulatedDistance;
extern uint32_t odometerTenths;

extern const int pulsesPerRevolution;
extern const float wheelDiameterInches;

// Speed + RPM
extern int speed;
extern int rpm;
extern int gearPosition;

// Fuel
extern int fuelLevel;

// Temperatures
extern int EGTemp;
extern int iaTemp;
extern int oilTemp;
extern int coolantTemp;
extern int transTemp;
extern int ambientTemp;

// Pressures
extern int oilPressure;
extern int fuelPressure;
extern int boostPressure;

// MPU6050
extern int accelerationX;
extern int accelerationY;
extern int accelerationZ;

// Power
extern int ignitionState;

// Timing
extern uint64_t previousMillis;
