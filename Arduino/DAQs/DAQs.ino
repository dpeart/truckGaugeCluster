#define DEBUG 1
#include "Globals.h"
#include "debug.h"
#include "SM_16UNIVIN.h"
#include "SM_RTD.h"  // Temp sensors
#include "SM_16DIGIN.h"
#include "Adafruit_FRAM_I2C.h"
#include "Adafruit_MCP9601.h"
#include "Adafruit_MPU6050.h"
#include "AuberinsSensors.h"  // Pressure sensors
#include "GaugePacket.h"
// #include "CruiseControl.h"

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

// I2C pins for ESP32-S3 Wire1 (adjust to your board / Pi header)
static const int SDA1_PIN = 3;  // Pi pin 3 equivalent
static const int SCL1_PIN = 5;  // Pi pin 5 equivalent

// FRAM Memory Map
// Odometer: 0x0

SM_16_UNIVIN* adc_card;
SM_16DIGIN* dig_card;
SM_RTD* rtd_card;

Adafruit_FRAM_I2C* fram;
Adafruit_MCP9601* mcp;
Adafruit_MPU6050* mpu;

// -------------------- ESP-NOW CALLBACK (optional debug) --------------------


void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}


// -------------------- SETUP --------------------

void setup() {
  // Basic debug serial (optional)
  Serial.begin(115200);
  delay(500);
  Serial.println("BOOTED");

  Wire1.begin(SDA1_PIN, SCL1_PIN, 400000);

  adc_card = new SM_16_UNIVIN(0, &Wire1);
  dig_card = new SM_16DIGIN(1, &Wire1);
  rtd_card = new SM_RTD(2, &Wire1);

  fram = new Adafruit_FRAM_I2C();
  mcp = new Adafruit_MCP9601();
  mpu = new Adafruit_MPU6050();

  // GPIOs
  pinMode(PWM_SPEED, INPUT);
  pinMode(PWM_TACH, INPUT);
  pinMode(SHUTDOWN, OUTPUT);

  // I2C for Sequent stack
  Wire1.begin(SDA1_PIN, SCL1_PIN, 400000);

  // ESP-NOW init
  WiFi.mode(WIFI_STA);  // 1. Put WiFi in STA mode

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) {
      delay(100);
    }
  }

  esp_now_register_send_cb(onEspNowSent);

  // Broadcast peer (all gauges listen)
  esp_now_peer_info_t peer = {};
  memset(&peer, 0, sizeof(peer));
  memset(peer.peer_addr, 0xFF, 6);  // broadcast MAC
  peer.channel = 1;
  peer.encrypt = false;

  // esp_wifi_set_promiscuous(true);                  // 2. Allow channel change
  // esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // 3. Set channel
  // esp_wifi_set_promiscuous(false);                 // 4. Lock it in place

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW peer");
  }

  // Initialize Sequent cards
  if (adc_card->begin()) {
    DB_PRINT("ADC Sixteen Universal Inputs Card detected\n");
  } else {
    DB_PRINT("ADC Sixteen Universal Inputs Card NOT detected!\n");
  }

  if (dig_card->begin()) {
    DB_PRINT("DIG Sixteen Digital Inputs Card detected\n");
  } else {
    DB_PRINT("DIG Sixteen Digital Inputs Card NOT detected!\n");
  }

  if (rtd_card->begin()) {
    DB_PRINT("RTD Inputs Card detected\n");
  } else {
    DB_PRINT("RTD Inputs Card NOT detected!\n");
  }

  // Initialize FRAM and read odometer value
  if (fram->begin(I2C_ODOMETER_ADR, &Wire1)) {
    DB_PRINTLN("Found I2C FRAM");
    fram->read(0, (uint8_t*)&odometer, sizeof(odometer));
    DB_PRINT("Initial Odometer Reading: ");
    DB_PRINTLN(odometer);
  } else {
    DB_PRINTLN("I2C FRAM not identified ... check your connections?");
  }

  // Initialize EGT reader
  if (!mcp->begin(I2C_EGT_ADR, &Wire1)) {
    DB_PRINTLN("Failed to find MCP9600 chip");
  } else {
    DB_PRINTLN("MCP9600 Found!");
    mcp->setThermocoupleType(MCP9600_TYPE_K);
    DB_PRINT("Thermocouple type: ");
    switch (mcp->getThermocoupleType()) {
      case MCP9600_TYPE_K:
        DB_PRINTLN("K");
        break;
      default:
        DB_PRINTLN("Unknown");
        break;
    }
  }

  // Initialize accelerometer
  if (!mpu->begin(I2C_ACCEL_ADR, &Wire1)) {
    DB_PRINTLN("Failed to find MPU6050 chip");
  } else {
    DB_PRINTLN("MPU6050 Found");
    mpu->setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    mpu->setMotionDetectionThreshold(1);
    mpu->setMotionDetectionDuration(20);
    mpu->setInterruptPinLatch(true);
    mpu->setInterruptPinPolarity(true);
    mpu->setMotionInterrupt(true);
  }
}

// -------------------- LOOP --------------------

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= 250) {
    previousMillis = currentMillis;

    if (DEBUG_SIMULATION_MODE) {
      generateDebugData();
    } else {
      readDigital();
      // speed = calculateSpeed();
      // rpm   = calculateRPM();
      readEGTemp();
      calculatePressures();
      calculateTemps();
      readAccelIfMotion();
      // updateMilesDriven(speed, 250); // if you want odometer from speed

      sendGaugePacket();  // ESP-NOW broadcast
    }
  }
}
// -------------------- ESP-NOW SEND --------------------

void sendGaugePacket() {
  GaugePacket pkt;

  pkt.speed = speed;
  pkt.rpm = rpm;
  pkt.gearPosition = gearPosition;

  pkt.iaTemp = iaTemp;
  pkt.oilTemp = oilTemp;
  pkt.coolantTemp = coolantTemp;
  pkt.transTemp = transTemp;
  pkt.ambientTemp = ambientTemp;
  pkt.EGTemp = EGTemp;

  pkt.oilPressure = oilPressure;
  pkt.fuelPressure = fuelPressure;
  pkt.boostPressure = boostPressure;

  pkt.accelerationX = accelerationX;
  pkt.accelerationY = accelerationY;
  pkt.accelerationZ = accelerationZ;

  pkt.digitalPins = digitalPins;

  pkt.cruiseActive = cruiseActive;
  pkt.cruiseSetValue = cruiseSetValue;

  esp_now_send(NULL, (uint8_t*)&pkt, sizeof(pkt));  // NULL = broadcast
}

// -------------------- SENSORS / CALCS --------------------

void calculatePressures() {
  oilPressure = calculatePressure5PSI(adc_card->readAnalogMv(ADC_OIL_PRESSURE));
  fuelPressure = calculatePressure5PSI(adc_card->readAnalogMv(ADC_FUEL_PRESSURE));
  boostPressure = calculatePressure5PSI(adc_card->readAnalogMv(ADC_BOOST_PRESSURE));
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
  iaTemp = int(rtd_card->readTemp(RTD_IA_TEMP) * INT_SCALING);
  oilTemp = int(rtd_card->readTemp(RTD_OIL_TEMP) * INT_SCALING);
  coolantTemp = int(rtd_card->readTemp(RTD_COOLANT_TEMP) * INT_SCALING);
  transTemp = int(rtd_card->readTemp(RTD_TRANS_TEMP) * INT_SCALING);
  ambientTemp = int(rtd_card->readTemp(RTD_AMBIENT_TEMP) * INT_SCALING);
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
  EGTemp = int(mcp->readThermocouple() * INT_SCALING);
  DB_PRINT("EGT:");
  DB_PRINTLN(EGTemp);
}

void readAccelIfMotion() {
  if (mpu->getMotionInterruptStatus()) {
    sensors_event_t a, g, temp;
    mpu->getEvent(&a, &g, &temp);

    accelerationX = int(a.acceleration.x * INT_SCALING);
    accelerationY = int(a.acceleration.y * INT_SCALING);
    accelerationZ = int(a.acceleration.z * INT_SCALING);

    DB_PRINT("AccelX:");
    DB_PRINT(accelerationX);
    DB_PRINT(",");
    DB_PRINT("AccelY:");
    DB_PRINT(accelerationY);
    DB_PRINT(",");
    DB_PRINT("AccelZ:");
    DB_PRINT(accelerationZ);
    DB_PRINT(", ");
    DB_PRINT("GyroX:");
    DB_PRINT(g.gyro.x);
    DB_PRINT(",");
    DB_PRINT("GyroY:");
    DB_PRINT(g.gyro.y);
    DB_PRINT(",");
    DB_PRINT("GyroZ:");
    DB_PRINT(g.gyro.z);
    DB_PRINTLN("");
  }
}

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
  digitalPins = dig_card->readInputs();
  DB_PRINT("Digital: ");
  DB_PRINTLN(digitalPins, BIN);
}

void readGearPosition() {
  gearPosition = adc_card->readAnalogMv(ADC_GEAR);
}

void generateDebugData() {
  static uint32_t t = 0;
  t += 1;  // increments every loop (250ms)

  // Speed 0–120 mph
  speed = (int)(60 + 60 * sin(t * 0.05));

  // RPM 800–6500
  rpm = (int)(3000 + 2500 * sin(t * 0.07));

  // Gear 1–6
  gearPosition = (int)(1 + (int)(3 + 2 * sin(t * 0.03)));

  // Temps (scaled)
  iaTemp = 200 + 50 * sin(t * 0.04);  // Intake air temp
  oilTemp = 220 + 40 * sin(t * 0.02);
  coolantTemp = 210 + 30 * sin(t * 0.025);
  transTemp = 190 + 20 * sin(t * 0.03);
  ambientTemp = 150 + 10 * sin(t * 0.01);
  EGTemp = 800 + 200 * sin(t * 0.05);  // EGT swings nicely

  // Pressures
  oilPressure = 40 + 10 * sin(t * 0.04);
  fuelPressure = 55 + 5 * sin(t * 0.03);
  boostPressure = 0 + 15 * sin(t * 0.06);

  // Acceleration
  accelerationX = 0 + 200 * sin(t * 0.10);
  accelerationY = 0 + 200 * sin(t * 0.13);
  accelerationZ = 1000 + 50 * sin(t * 0.02);

  // Digital pins (simulate blink)
  digitalPins = (t % 8 == 0) ? 0xAAAA : 0x5555;

  // Cruise
  cruiseActive = (t % 20 < 10);
  cruiseSetValue = 65;
}
