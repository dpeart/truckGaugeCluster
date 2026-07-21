#define DEBUG 1
#include "debug.h"
#include <Arduino.h>
#include "Globals.h"
#include "GaugePacket.h"
#include "SM_16UNIVIN.h"
#include "SM_RTD.h"
#include "SM_16DIGIN.h"
#include "Adafruit_FRAM_I2C.h"
#include "Adafruit_MCP9601.h"
#include "Adafruit_MPU6050.h"
#include "AuberinsSensors.h"
#include "CruiseControl.h"
#include "GNSS.h"
#include "interruptHandlers.h"

#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

bool DEBUG_SIMULATION_MODE = true;

// -------------------- STATE --------------------
unsigned int cruiseAccel = 0;
unsigned int cruiseActive = 0;
unsigned int cruiseDecel = 0;
unsigned int cruiseSetValue = 0;
unsigned int cruiseSpeedActive = 0;

unsigned int digitalPins = 0;

uint32_t odometer = 0;

int gearPosition = 0;
int fuelLevel = 0;
int batteryLevel = 0;

int EGTemp = 0;
int iaTemp = 0;
int oilTemp = 0;
int coolantTemp = 0;
int transTemp = 0;
int ambientTemp = 0;

int oilPressure = 0;
int fuelPressure = 0;
int boostPressure = 0;

int accelerationX = 0;
int accelerationY = 0;
int accelerationZ = 0;

unsigned long previousMillis = 0;
unsigned long previousGPS = 0;

// I2C pins
static const int SDA1_PIN = 3;
static const int SCL1_PIN = 5;

// Sequent cards
SM_16_UNIVIN adc_card(0, &Wire1);
SM_16DIGIN dig_card(1, &Wire1);
SM_RTD rtd_card(2, &Wire1);

Adafruit_FRAM_I2C fram;
Adafruit_MCP9601 mcp;
Adafruit_MPU6050 mpu;
GNSSModule gps;

static GaugePacket pkt;

static uint8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// -------------------- ESP-NOW INIT --------------------
void initEspNow() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_wifi_start() == ESP_OK)
    Serial.println("wifi_start OK");
  else
    Serial.println("wifi_start FAIL");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, bcast, 6);
  peer.channel = 1;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;

  esp_now_add_peer(&peer);
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  gps.begin();

  pinMode(PWM_SPEED, INPUT_PULLUP);
  pinMode(PWM_TACH, INPUT_PULLUP);

  initInterruptHandlers();

  Wire1.begin(SDA1_PIN, SCL1_PIN, 400000);

  initEspNow();

  if (adc_card.begin()) DB_PRINT("ADC card OK\n");
  if (dig_card.begin()) DB_PRINT("DIG card OK\n");
  if (rtd_card.begin()) DB_PRINT("RTD card OK\n");

  if (fram.begin(I2C_ODOMETER_ADR, &Wire1)) {
    fram.read(0, (uint8_t *)&odometer, sizeof(odometer));
    DB_PRINT("Initial Odometer: ");
    DB_PRINTLN(odometer);
  }

  if (mcp.begin(I2C_EGT_ADR, &Wire1)) {
    mcp.setThermocoupleType(MCP9600_TYPE_K);
  }
}

enum SensorTask {
  TASK_DIGITAL,
  TASK_EGT,
  TASK_PRESSURES,
  TASK_TEMPS,
  TASK_GEAR,
  TASK_COUNT
};

uint8_t currentTask = 0;

// -------------------- LOOP --------------------
void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousGPS >= 1000) {
    previousGPS = currentMillis;
    gps.update(pkt);
  }

  if (currentMillis - previousMillis >= 16) {
    previousMillis = currentMillis;

    float speedLocal;
    float rpmLocal;
    uint32_t odoLocal;

    speedLocal = g_speedMph;
    odoLocal   = g_odometerTenths;

    rpmLocal = g_rpm;

    if (DEBUG_SIMULATION_MODE) {
      generateDebugData();

      fillGaugePacket(pkt,
                      (int)speedLocal,
                      (int)rpmLocal,
                      odoLocal,
                      gearPosition,
                      fuelLevel,
                      batteryLevel,
                      iaTemp, oilTemp, coolantTemp,
                      transTemp, ambientTemp, EGTemp,
                      oilPressure, fuelPressure, boostPressure,
                      accelerationX, accelerationY, accelerationZ,
                      digitalPins,
                      cruiseActive, cruiseSetValue);

      gps.update(pkt);
      esp_now_send(bcast, (uint8_t *)&pkt, sizeof(pkt));
    } else {

      switch (currentTask) {
        case TASK_DIGITAL:   readDigital();        break;
        case TASK_EGT:       readEGTemp();         break;
        case TASK_PRESSURES: calculatePressures(); break;
        case TASK_TEMPS:     calculateTemps();     break;
        case TASK_GEAR:      readGearPosition();   break;
      }

      currentTask = (currentTask + 1) % TASK_COUNT;

      fillGaugePacket(pkt,
                      (int)speedLocal,
                      (int)rpmLocal,
                      odoLocal,
                      gearPosition,
                      fuelLevel,
                      batteryLevel,
                      iaTemp, oilTemp, coolantTemp,
                      transTemp, ambientTemp, EGTemp,
                      oilPressure, fuelPressure, boostPressure,
                      accelerationX, accelerationY, accelerationZ,
                      digitalPins,
                      cruiseActive, cruiseSetValue,
                      pkt.year, pkt.month, pkt.day,
                      pkt.hour, pkt.minute, pkt.second,
                      pkt.headingDeg / 100.0f,
                      pkt.compass8);

      esp_now_send(bcast, (uint8_t *)&pkt, sizeof(pkt));
    }
  }
}

// -------------------- SENSOR FUNCTIONS --------------------
void calculatePressures() {
  oilPressure = calculatePressure5PSI(adc_card.readAnalogMv(ADC_OIL_PRESSURE));
  fuelPressure = calculatePressure5PSI(adc_card.readAnalogMv(ADC_FUEL_PRESSURE));
  boostPressure = calculatePressure5PSI(adc_card.readAnalogMv(ADC_BOOST_PRESSURE));
}

void calculateTemps() {
  iaTemp = int(rtd_card.readTemp(RTD_IA_TEMP) * INT_SCALING);
  oilTemp = int(rtd_card.readTemp(RTD_OIL_TEMP) * INT_SCALING);
  coolantTemp = int(rtd_card.readTemp(RTD_COOLANT_TEMP) * INT_SCALING);
  transTemp = int(rtd_card.readTemp(RTD_TRANS_TEMP) * INT_SCALING);
  ambientTemp = int(rtd_card.readTemp(RTD_AMBIENT_TEMP) * INT_SCALING);
}

void readEGTemp() {
  EGTemp = int(mcp.readThermocouple() * INT_SCALING);
}

void readDigital() {
  digitalPins = dig_card.readInputs();
}

void readGearPosition() {
  gearPosition = adc_card.readAnalogMv(ADC_GEAR);
}

void simulateDigitalPins() {
  static uint16_t bit = 1;
  static int64_t lastChange = 0;

  int64_t now = esp_timer_get_time();

  if (now - lastChange < 500000) return;

  lastChange = now;
  digitalPins = bit;

  bit <<= 1;
  if (bit == 0 || bit > (1 << 15)) bit = 1;
}

void generateDebugData() {
  static uint32_t t = 0;
  t++;

  gearPosition = (int)(1 + (int)(3 + 2 * sin(t * 0.03)));

  fuelLevel = 50 + 50 * sin(t * 0.04);
  batteryLevel = 8 + 8 * sin(t * 0.04);

  iaTemp = 125 + 75 * sin(t * 0.04);
  oilTemp = 225 + 75 * sin(t * 0.05);
  coolantTemp = 200 + 50 * sin(t * 0.05);
  transTemp = 190 + 20 * sin(t * 0.03);
  ambientTemp = 65 + 85 * sin(t * 0.01);
  EGTemp = 800 + 700 * sin(t * 0.05);

  oilPressure = 45 + 45 * sin(t * 0.04);
  fuelPressure = 45 + 25 * sin(t * 0.03);
  boostPressure = 30 + 30 * sin(t * 0.06);

  simulateDigitalPins();
}
