#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_log.h"
#include <esp_timer.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "Globals.h"
#include "GaugePacket.h"
#include "GNSS.h"
#include "CruiseControl.h"
#include "interruptHandlers.h"

#include "AuberinsSensors.h"
#include "SM_16UNIVIN.h"
#include "SM_16DIGIN.h"
#include "SM_RTD.h"
#include "MCP9601.h"

// -------------------- LOGGING --------------------
static const char *TAG = "GaugeCluster";

// -------------------- STATE --------------------
bool DEBUG_SIMULATION_MODE = true;

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

uint64_t previousMillis = 0;
uint64_t previousGPS = 0;

// -------------------- I2C CONFIG --------------------
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_FREQ_HZ 400000

static void i2c_master_init(void)
{
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master = {.clk_speed = I2C_MASTER_FREQ_HZ},
      .clk_flags = 0,
  };

  ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));

  ESP_LOGI(TAG, "I2C initialized on SDA=%d SCL=%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
}
// -------------------- ESP-NOW INIT --------------------
static uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void initEspNow()
{
  // 1. Initialize NVS (Required by the Wi-Fi driver)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // 2. Initialize TCP/IP stack and default event loop
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  // 3. Initialize Wi-Fi driver with default configuration
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // 4. Set Wi-Fi storage to RAM and set Station mode
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  // 5. Initialize ESP-NOW
  ESP_ERROR_CHECK(esp_now_init());
  esp_now_peer_info_t peerInfo = {};
  // Example broadcast address, or replace with your receiver's MAC
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0; // Use current channel
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to add ESP-NOW peer");
  } else {
    ESP_LOGI(TAG, "ESP-NOW peer added successfully");
  }
}

SM_16_UNIVIN adc_card(0);
SM_16DIGIN dig_card(1);
SM_RTD rtd_card(2);
MCP9601 mcp(I2C_EGT_ADR);
GNSSModule gps;

// -------------------- SENSOR PLACEHOLDERS --------------------
void readDigital()
{
  // SM_16DIGIN → returns 16-bit packed digital input state
  int val = dig_card.readInputs();
  if (val >= 0)
    digitalPins = val;
  else
    digitalPins = 0; // fallback
}

void readEGTemp()
{
  // MCP9601 → returns float °C
  float t = mcp.readThermocouple();
  if (t > -500.0f) // MCP9601 error codes are around -1000
    EGTemp = int(t * INT_SCALING);
  else
    EGTemp = 0;
}

void calculatePressures()
{
  // SM_16UNIVIN → analog millivolts
  int oilMv = adc_card.readAnalogMv(ADC_OIL_PRESSURE);
  int fuelMv = adc_card.readAnalogMv(ADC_FUEL_PRESSURE);
  int boostMv = adc_card.readAnalogMv(ADC_BOOST_PRESSURE);

  oilPressure = calculatePressure5PSI(oilMv);
  fuelPressure = calculatePressure5PSI(fuelMv);
  boostPressure = calculatePressure5PSI(boostMv);
}

void calculateTemps()
{
  // SM_RTD → returns float °C
  iaTemp = int(rtd_card.readTemp(RTD_IA_TEMP) * INT_SCALING);
  oilTemp = int(rtd_card.readTemp(RTD_OIL_TEMP) * INT_SCALING);
  coolantTemp = int(rtd_card.readTemp(RTD_COOLANT_TEMP) * INT_SCALING);
  transTemp = int(rtd_card.readTemp(RTD_TRANS_TEMP) * INT_SCALING);
  ambientTemp = int(rtd_card.readTemp(RTD_AMBIENT_TEMP) * INT_SCALING);
}

void readGearPosition()
{
  // SM_16UNIVIN → analog millivolts
  int mv = adc_card.readAnalogMv(ADC_GEAR);
  gearPosition = mv; // your original code used raw mV
}

// -------------------- DEBUG SIMULATION --------------------
void simulateDigitalPins()
{
  static uint16_t bit = 1;
  static int64_t lastChange = 0;

  int64_t now = esp_timer_get_time();

  if (now - lastChange < 500000)
    return;

  lastChange = now;
  digitalPins = bit;

  bit <<= 1;
  if (bit == 0 || bit > (1 << 15))
    bit = 1;
}

void generateDebugData()
{
  static uint32_t t = 0;
  t++;

  gearPosition = (int)(1 + (int)(3 + 2 * sin(t * 0.03)));

  fuelLevel = 50 + 50 * sin(t * 0.04);
  batteryLevel = 8 + 8 * sin(t * 0.04);

  iaTemp = 125 + 75 * sin(t * 0.04);
  oilTemp = 225 + 75 * sin(t * 0.05);
  coolantTemp = 200 + 50 * sin(t * 0.05);
  transTemp = 170 + 90 * sin(t * 0.03);
  ambientTemp = 65 + 85 * sin(t * 0.01);
  EGTemp = 800 + 700 * sin(t * 0.05);

  oilPressure = 45 + 45 * sin(t * 0.04);
  fuelPressure = 45 + 25 * sin(t * 0.03);
  boostPressure = 30 + 30 * sin(t * 0.06);

  simulateDigitalPins();
}

void scanI2CBus(i2c_port_t port)
{
  ESP_LOGI("I2C_SCAN", "Scanning I2C bus...");
  int devices_found = 0;
  for (uint8_t addr = 1; addr < 127; addr++)
  {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK)
    {
      ESP_LOGI("I2C_SCAN", "Found device at address: 0x%02X", addr);
      devices_found++;
    }
  }
  if (devices_found == 0)
  {
    ESP_LOGW("I2C_SCAN", "No I2C devices found!");
  }
}

// -------------------- MAIN --------------------
extern "C" void app_main(void)
{
  ESP_LOGI(TAG, "Starting ESP32-Pi Gauge Cluster");

  i2c_master_init();
  scanI2CBus(I2C_NUM_0);
  initEspNow();
  initInterruptHandlers();

  dig_card.begin();
  adc_card.begin();
  rtd_card.begin();
  mcp.begin();
  gps.begin();

  GaugePacket pkt;

  previousMillis = esp_timer_get_time();
  previousGPS = esp_timer_get_time();

  while (true)
  {
    uint64_t now = esp_timer_get_time();

    if (now - previousGPS >= 1000000ULL)
    {
      previousGPS = now;
      gps.update(pkt);
    }

    if (now - previousMillis >= 16000ULL)
    {
      previousMillis = now;

      float speedLocal = g_speedMph;
      float rpmLocal = g_rpm;
      uint32_t odoLocal = g_odometerTenths;

      if (DEBUG_SIMULATION_MODE)
      {
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
      }
      else
      {
        static uint8_t currentTask = 0;

        switch (currentTask)
        {
        case 0:
          readDigital();
          break;
        case 1:
          readEGTemp();
          break;
        case 2:
          calculatePressures();
          break;
        case 3:
          calculateTemps();
          break;
        case 4:
          readGearPosition();
          break;
        }

        currentTask = (currentTask + 1) % 5;

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

    vTaskDelay(1);
  }
}
