#define DEBUG
#define X4

// I2C Addresses
#define I2C_ACCEL_ADR 0x68       // Known
#define I2C_EGT_ADR 0x67       // Known
#define I2C_ODOMETER_ADR 0x50  // Known
#define ADC_CARD 0x58          // Known
#define DIG_CARD 0x5a
#define RTD_CARD 0x5c
#define I2C_BATTERY_ADR 0x48

// DIG Card Pins
#define DIG_OVER_DRIVE 1
#define DIG_TCC 2
#define DIG_LEFT 3
#define DIG_RIGHT 4
#define DIG_BRAKE 5
#define DIG_HEAD_LOW 6
#define DIG_HEAD_HIGH 7
#define DIG_RUNNING 8
#define DIG_WATER_FUEL 9
#define DIG_LOW_WASHER 10
#define DIG_CRUISE_ON 11
#define DIG_CRUISE_SET 12
#define DIG_CRUISE_RESUME 13
#define DIG_BRAKE_LIGHT 14
#define DIG_IGNITION 15

// RP2040 GPIO PWN Pins
#define PWM_SPEED 13
#define PWM_TACH 19

#define SHUTDOWN 14

// ADC Card Pins
// For now use only odd pins, as they have a shared ground and it is difficult to fit two grounds into the same socket
#define ADC_GEAR 2
#define ADC_TPS 3
#define ADC_BOOST_PRESSURE 10
#define ADC_OIL_PRESSURE 12
#define ADC_FUEL_PRESSURE 14
#define ADC_FUEL_LEVEL 4

// RTD Card Pins
#define RTD_IA_TEMP 1
#define RTD_OIL_TEMP 2
#define RTD_TRANS_TEMP 3
#define RTD_COOLANT_TEMP 4
#define RTD_AMBIENT_TEMP 5

// Amount to multiply a float by to convert to int, will be divided in custom xml
#define INT_SCALING 100


extern int dig[];

// defined by cruise state
extern unsigned int cruiseAccel;
extern unsigned int cruiseActive;
extern unsigned int cruiseDecel;
extern unsigned int cruiseSetValue;
extern unsigned int cruiseSpeedActive;

// define digitalPins
extern unsigned int digitalPins;

// Odometer
extern int totalMiles;           // in 1/10 of a mile
extern int odometer;             // Current odometer reading in 1/10 of a mile
extern int accumulatedDistance;  // Accumulated distance since last odometer update in 1/10 of a mile

extern const int pulsesPerRevolution;      // Number of pulses per wheel revolution
extern const float wheelDiameterInches;  // Wheel diameter in inches (example value)

extern int speed;
extern int rpm;
extern int gearPosition;

// Temp
extern int EGTemp;
extern int iaTemp;
extern int oilTemp;
extern int coolantTemp;
extern int transTemp;
extern int ambientTemp;

// Pressure
extern int oilPressure;
extern int fuelPressure;
extern int boostPressure;

//MPU6050
extern int accelerationX;
extern int accelerationY;
extern int accelerationZ;

// Power
extern int ignitionState;

extern unsigned long previousMillis;  // Stores the last time the procedure was called
