#define DEBUG 1
// #include "config.h"
#include "debug.h"
#include "Globals.h"
#include "GaugePacket.h"
#include "SM_16UNIVIN.h"
#include "SM_RTD.h"  // Temp sensors
#include "SM_16DIGIN.h"
#include "Adafruit_FRAM_I2C.h"
#include "Adafruit_MCP9601.h"
#include "Adafruit_MPU6050.h"
#include "AuberinsSensors.h"  // Pressure sensors
#include "CruiseControl.h"
#include "GNSS.h"

#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

bool DEBUG_SIMULATION_MODE = true;  // set false to use real DAQ data

// -------------------- CRUISE / STATE --------------------

unsigned int cruiseAccel = 0;
unsigned int cruiseActive = 0;
unsigned int cruiseDecel = 0;
unsigned int cruiseSetValue = 0;
unsigned int cruiseSpeedActive = 0;

// digital pins
unsigned int digitalPins = 0;

// Used to calculate Speed
volatile unsigned long lastTimeVSS = 0;
volatile unsigned long periodVSS = 0;
volatile unsigned long lastTimeTACH = 0;
volatile unsigned long periodTACH = 0;

// Odometer
int totalMiles = 0;           // in number of 1/10 of mile
int odometer = 0;             // Current odometer reading in 1/10 of a mile
int accumulatedDistance = 0;  // Accumulated distance since last odometer update

const int vssPulsesPerRevolution = 48;   // Number of pulses per wheel revolution
const int tachPulsesPerRevolution = 48;  // Number of pulses per wheel revolution
const float wheelDiameterInches = 20.0;  // Wheel diameter in inches (example value)

int speed = 0;
int rpm = 0;
int gearPosition = 0;
int fuelLevel = 0;
int batteryLevel = 0;

// Temp
int EGTemp = 0;
int iaTemp = 0;
int oilTemp = 0;
int coolantTemp = 0;
int transTemp = 0;
int ambientTemp = 0;

// Pressure (100 * float(pressure))
int oilPressure = 0;
int fuelPressure = 0;
int boostPressure = 0;

// MPU6050
int accelerationX = 0;
int accelerationY = 0;
int accelerationZ = 0;

// Power
int ignitionState = 0;

unsigned long previousMillis = 0;  // Stores the last time the procedure was called
unsigned long previousGPS = 0;     // Stores the last time the procedure was called

// I2C pins for ESP32-S3 Wire1 (adjust to your board / Pi header)
static const int SDA1_PIN = 3;  // Pi pin 3 equivalent
static const int SCL1_PIN = 5;  // Pi pin 5 equivalent

// FRAM Memory Map
// Odometer: 0x0

// Sequent cards on Wire1
SM_16_UNIVIN adc_card(0, &Wire1);
SM_16DIGIN dig_card(1, &Wire1);
SM_RTD rtd_card(2, &Wire1);

Adafruit_FRAM_I2C fram = Adafruit_FRAM_I2C();  // FRAM
Adafruit_MCP9601 mcp;                          // EGT
Adafruit_MPU6050 mpu;                          // Accelerometer
GNSSModule gps;

static GaugePacket pkt;

// Broadcast MAC (shared by all sends)
static uint8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// -------------------- ESP-NOW CALLBACK (optional debug) --------------------

void onEspNowSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Serial.print("SEND CALLBACK: ");
  DB_PRINTLN(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");

  if (info) {
    Serial.print("SRC: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", info->src_addr[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();

    Serial.print("DEST: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", info->des_addr[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();

    Serial.print("TX status: ");
    Serial.println(info->tx_status);

    Serial.print("Rate: ");
    Serial.println(info->rate);
  }
}

// -------------------- ESP-NOW INIT (IDF 5.x / Arduino 3.4.x) --------------------

void initEspNow() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();

  // Set channel BEFORE wifi_start()
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_wifi_start() == ESP_OK)
    Serial.println("wifi_start OK");
  else
    Serial.println("wifi_start FAIL");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  Serial.println("ESP-NOW init success");

  // esp_now_register_send_cb(onEspNowSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, bcast, 6);
  peer.channel = 1;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;  // REQUIRED in IDF 5.x

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW peer");
  } else {
    Serial.println("Broadcast peer added");
  }
}

// -------------------- SETUP --------------------

void setup() {
  // Basic debug serial (optional)
  Serial.begin(115200);
  delay(500);
  gps.begin();

  // GPIOs
  pinMode(PWM_SPEED, INPUT);
  pinMode(PWM_TACH, INPUT);
  // pinMode(SHUTDOWN, OUTPUT);

  // I2C for Sequent stack
  Wire1.begin(SDA1_PIN, SCL1_PIN, 400000);

  // ESP-NOW / WiFi
  initEspNow();

  // Initialize Sequent cards
  if (adc_card.begin()) {
    DB_PRINT("ADC Sixteen Universal Inputs Card detected\n");
  } else {
    DB_PRINT("ADC Sixteen Universal Inputs Card NOT detected!\n");
  }

  if (dig_card.begin()) {
    DB_PRINT("DIG Sixteen Digital Inputs Card detected\n");
  } else {
    DB_PRINT("DIG Sixteen Digital Inputs Card NOT detected!\n");
  }

  if (rtd_card.begin()) {
    DB_PRINT("RTD Inputs Card detected\n");
  } else {
    DB_PRINT("RTD Inputs Card NOT detected!\n");
  }

  // Initialize FRAM and read odometer value
  if (fram.begin(I2C_ODOMETER_ADR, &Wire1)) {
    DB_PRINTLN("Found I2C FRAM");
    fram.read(0, (uint8_t *)&odometer, sizeof(odometer));
    DB_PRINT("Initial Odometer Reading: ");
    DB_PRINTLN(odometer);
  } else {
    DB_PRINTLN("I2C FRAM not identified ... check your connections?");
  }

  // Initialize EGT reader
  if (!mcp.begin(I2C_EGT_ADR, &Wire1)) {
    DB_PRINTLN("Failed to find MCP9600 chip");
  } else {
    DB_PRINTLN("MCP9600 Found!");
    mcp.setThermocoupleType(MCP9600_TYPE_K);
    DB_PRINT("Thermocouple type: ");
    switch (mcp.getThermocoupleType()) {
      case MCP9600_TYPE_K:
        DB_PRINTLN("K");
        break;
      default:
        DB_PRINTLN("Unknown");
        break;
    }
  }

  // Initialize accelerometer
  //   if (!mpu.begin(I2C_ACCEL_ADR, &Wire1)) {
  //     DB_PRINTLN("Failed to find MPU6050 chip");
  //   } else {
  //     DB_PRINTLN("MPU6050 Found");
  //     mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
  //     mpu.setMotionDetectionThreshold(1);
  //     mpu.setMotionDetectionDuration(20);
  //     mpu.setInterruptPinLatch(true);
  //     mpu.setInterruptPinPolarity(true);
  //     mpu.setMotionInterrupt(true);
  //   }
}

// -------------------- LOOP --------------------

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousGPS >= 1000) {
    previousGPS = currentMillis;
    gps.update(pkt);

    // --- Local Time Conversion ---
    int lh, lm, ls, lmo, ld, ly;
    gps.getLocalTime(lh, lm, ls, lmo, ld, ly);
    // DB_PRINTF("Local Time: %04d-%02d-%02d %02d:%02d:%02d\n", ly, lmo, ld, lh, lm, ls);

    // // GNSS Debug Output //
    // DB_PRINTLN("----- GNSS -----");

    // DB_PRINTF("UTC: %02d:%02d:%02d\n",
    //           gps.utc.hour, gps.utc.minute, gps.utc.second);

    // DB_PRINTF("Date: %04d-%02d-%02d\n",
    //           gps.date.year, gps.date.month, gps.date.date);

    // DB_PRINTF("Lat: %.6f\n", gps.latitudeDeg());
    // DB_PRINTF("Lon: %.6f\n", gps.longitudeDeg());
    // DB_PRINTF("Alt: %.2f m\n", gps.altitude);

    // DB_PRINTF("Sats Used: %d\n", gps.satsUsed);
    // DB_PRINTF("SOG: %.2f\n", gps.sog);
    // DB_PRINTF("COG: %.2f\n", gps.cog);
    // DB_PRINTF("Mode: %d\n", gps.mode);

    // // --- NEW: Heading + Compass Direction ---
    // DB_PRINTF("Heading: %.2f deg (%s)\n", gps.getHeading(), gps.getCompass8());

    if (gps.hasFix()) {
      DB_PRINTLN("Fix: YES (3D)");
    } else {
      DB_PRINTLN("Fix: NO");
    }
  }


  if (currentMillis - previousMillis >= 16) {
    previousMillis = currentMillis;

    if (DEBUG_SIMULATION_MODE) {

      // 1. Generate simulated vehicle data
      generateDebugData();

      // 2. Fill ONLY the vehicle fields
      fillGaugePacket(pkt,
                      speed, rpm, gearPosition,
                      fuelLevel,  // <-- add this
                      batteryLevel,  // <-- add this
                      iaTemp, oilTemp, coolantTemp,
                      transTemp, ambientTemp, EGTemp,
                      oilPressure, fuelPressure, boostPressure,
                      accelerationX, accelerationY, accelerationZ,
                      digitalPins,
                      cruiseActive, cruiseSetValue);

      // 3. Fill GNSS fields using the real GNSS module
      gps.update(pkt);

      printGaugePacket(pkt);

      // 4. Send the fully populated packet
      esp_now_send(bcast, (uint8_t *)&pkt, sizeof(pkt));

    } else {

      // REAL MODE — unchanged
      readDigital();
      readEGTemp();
      calculatePressures();
      calculateTemps();

      // GNSS update happens inside sendGaugePacket()
      sendGaugePacket();
    }
  }
}

// -------------------- ESP-NOW SEND --------------------
void sendGaugePacket() {
  // pkt is the persistent GaugePacket updated by GNSS every 1000ms
  // and by DAQ every 250ms

  fillGaugePacket(
    pkt,
    speed,
    rpm,
    gearPosition,
    fuelLevel,  // <-- add this
    batteryLevel,  // <-- add this
    iaTemp,
    oilTemp,
    coolantTemp,
    transTemp,
    ambientTemp,
    EGTemp,
    oilPressure,
    fuelPressure,
    boostPressure,
    accelerationX,
    accelerationY,
    accelerationZ,
    digitalPins,
    cruiseActive,
    cruiseSetValue,
    pkt.year,
    pkt.month,
    pkt.day,
    pkt.hour,
    pkt.minute,
    pkt.second,
    pkt.headingDeg / 100.0f,
    pkt.compass8);

  esp_now_send(bcast, (uint8_t *)&pkt, sizeof(pkt));
}


// -------------------- SENSORS / CALCS --------------------

// (everything below here is unchanged)

void calculatePressures() {
  oilPressure = calculatePressure5PSI(adc_card.readAnalogMv(ADC_OIL_PRESSURE));
  fuelPressure = calculatePressure5PSI(adc_card.readAnalogMv(ADC_FUEL_PRESSURE));
  boostPressure = calculatePressure5PSI(adc_card.readAnalogMv(ADC_BOOST_PRESSURE));
  DB_PRINT("oilP:");
  DB_PRINT(oilPressure);
  DB_PRINT(",");
  DB_PRINT("fuelP:");
  DB_PRINT(fuelPressure);
  DB_PRINT(",");
  DB_PRINT("boostP:");
  DB_PRINT(boostPressure);
  DB_PRINTLN("");
}

void calculateTemps() {
  iaTemp = int(rtd_card.readTemp(RTD_IA_TEMP) * INT_SCALING);
  oilTemp = int(rtd_card.readTemp(RTD_OIL_TEMP) * INT_SCALING);
  coolantTemp = int(rtd_card.readTemp(RTD_COOLANT_TEMP) * INT_SCALING);
  transTemp = int(rtd_card.readTemp(RTD_TRANS_TEMP) * INT_SCALING);
  ambientTemp = int(rtd_card.readTemp(RTD_AMBIENT_TEMP) * INT_SCALING);
  DB_PRINT("iaT:");
  DB_PRINT(iaTemp);
  DB_PRINT(",");
  DB_PRINT("oilT:");
  DB_PRINT(oilTemp);
  DB_PRINT(",");
  DB_PRINT("coolantT:");
  DB_PRINT(coolantTemp);
  DB_PRINT(",");
  DB_PRINT("transT:");
  DB_PRINT(transTemp);
  DB_PRINT(",");
  DB_PRINT("ambientT:");
  DB_PRINT(ambientTemp);
  DB_PRINTLN("");
}

void readEGTemp() {
  EGTemp = int(mcp.readThermocouple() * INT_SCALING);
  DB_PRINT("EGT:");
  DB_PRINTLN(EGTemp);
}

// void readAccelIfMotion() {
//   if (mpu.getMotionInterruptStatus()) {
//     sensors_event_t a, g, temp;
//     mpu.getEvent(&a, &g, &temp);

//     accelerationX = int(a.acceleration.x * INT_SCALING);
//     accelerationY = int(a.acceleration.y * INT_SCALING);
//     accelerationZ = int(a.acceleration.z * INT_SCALING);

//     DB_PRINT("AccelX:");
//     DB_PRINT(accelerationX);
//     DB_PRINT(",");
//     DB_PRINT("AccelY:");
//     DB_PRINT(accelerationY);
//     DB_PRINT(",");
//     DB_PRINT("AccelZ:");
//     DB_PRINT(accelerationZ);
//     DB_PRINT(", ");
//     DB_PRINT("GyroX:");
//     DB_PRINT(g.gyro.x);
//     DB_PRINT(",");
//     DB_PRINT("GyroY:");
//     DB_PRINT(g.gyro.y);
//     DB_PRINT(",");
//     DB_PRINT("GyroZ:");
//     DB_PRINT(g.gyro.z);
//     DB_PRINTLN("");
//   }
// }

void updateMilesDriven(float speed, unsigned long timeMillis) {
  float timeHours = timeMillis / 3600000.0;
  int distance = int(speed * timeHours * 10);
  totalMiles += distance;
  accumulatedDistance += distance;

  if (accumulatedDistance >= 1) {
    odometer += accumulatedDistance;
    accumulatedDistance = 0;
  }

  // fram.write(0, (uint8_t*)&odometer, sizeof(odometer));
  DB_PRINT("ODO: ");
  DB_PRINTLN(odometer);
  DB_PRINT("TRIP: ");
  DB_PRINTLN(accumulatedDistance);
}

void measurePeriodVSS() {
  unsigned long currentTime = millis();
  periodVSS = currentTime - lastTimeVSS;
  lastTimeVSS = currentTime;
}

float calculateSpeed() {
  noInterrupts();
  unsigned long periodMillis = periodVSS;
  interrupts();

  if (periodMillis > 0) {
    float periodSeconds = (periodMillis * vssPulsesPerRevolution) / 1000.0;
    float circumferenceInches = 2 * PI * wheelDiameterInches;
    float velocityInchesPerSec = circumferenceInches / periodSeconds;
    float velocityMph = (velocityInchesPerSec * 3600) / (12 * 5280);

    DB_PRINT("Speed:");
    DB_PRINTLN(velocityMph);
    return velocityMph;
  } else {
    return -1;
  }
}

void measurePeriodTACH() {
  unsigned long currentTime = millis();
  periodTACH = currentTime - lastTimeTACH;
  lastTimeTACH = currentTime;
}

float calculateRPM() {
  noInterrupts();
  unsigned long periodMillis = periodTACH;
  interrupts();

  if (periodMillis > 0) {
    float frequency = 1.0 / (periodMillis / 1000.0);
    float rpmVal = (frequency * 60.0) / tachPulsesPerRevolution;
    DB_PRINT("RPM: ");
    DB_PRINTLN(rpmVal);
    return rpmVal;
  } else {
    return -1;
  }
}

void readDigital() {
  digitalPins = dig_card.readInputs();
  DB_PRINT("Digital: ");
  DB_PRINTLN(digitalPins, BIN);
}

void readGearPosition() {
  gearPosition = adc_card.readAnalogMv(ADC_GEAR);
}

void simulateDigitalPins() {
  static uint16_t bit = 1;  // start at IND_OD (bit 0)
  static int64_t lastChange = 0;

  int64_t now = esp_timer_get_time();  // microseconds

  if (now - lastChange < 500000)  // change every 0.5 sec
    return;

  lastChange = now;

  digitalPins = bit;

  // Shift left; wrap back to bit 0 after bit 15
  bit <<= 1;
  if (bit == 0 || bit > (1 << 15))
    bit = 1;
}

void generateDebugData() {
  static uint32_t t = 0;
  t += 1;  // increments every loop (250ms)

  // Speed 0–140 mph
  static float speed_theta = 0.0f;
  speed_theta += 0.006702f;  // one update every 16ms
  speed = (int)(705.0f + 705.0f * sinf(speed_theta));

  // RPM 0–5000
  static float tach_theta = 0.0f;
  tach_theta += 0.03702f;  // one update every 16ms

  rpm = (int)(2500 + 2500 * sin(tach_theta));

  // Gear 1–6
  gearPosition = (int)(1 + (int)(3 + 2 * sin(t * 0.03)));

  fuelLevel = 50 + 50 * sin(t * 0.04);  // Fuel Level
  batteryLevel = 8 + 8 * sin(t * 0.04);  // Fuel Level

  // Temps (scaled)
  iaTemp = 125 + 75 * sin(t * 0.04);  // Intake air temp
  oilTemp = 220 + 40 * sin(t * 0.05);
  coolantTemp = 200 + 50 * sin(t * 0.05);
  transTemp = 190 + 20 * sin(t * 0.03);
  ambientTemp = 150 + 10 * sin(t * 0.01);
  EGTemp = 800 + 700 * sin(t * 0.05);  // EGT swings nicely

  // Pressures
  oilPressure = 45 + 45 * sin(t * 0.04);
  fuelPressure = 55 + 5 * sin(t * 0.03);
  boostPressure = 0 + 15 * sin(t * 0.06);

  // Acceleration
  // accelerationX = 0 + 200 * sin(t * 0.10);
  // accelerationY = 0 + 200 * sin(t * 0.13);
  // accelerationZ = 1000 + 50 * sin(t * 0.02);

  // Digital pins (simulate blink)
  // digitalPins = (t % 8 == 0) ? 0xAAAA : 0x5555;
  simulateDigitalPins();
  // Cruise
  // cruiseActive = (t % 20 < 10);
  // cruiseSetValue = 65;
}
