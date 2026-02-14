#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "GaugePacket.h"  // <-- includes your struct + printGaugePacket()

// UART pins for Waveshare ESP32-P4-LCD-4C
// P4 RX = GPIO 18 (connect S3 TX here)
static constexpr int UART_RX_PIN = -1;
static constexpr int UART_TX_PIN = 17;  // unused

volatile bool newPacket = false;
GaugePacket currentPacket;

void forwardToP4(const GaugePacket &pkt) {
  const uint8_t START_BYTE = 0xAA;
  const uint8_t END_BYTE = 0x55;

  Serial2.write(START_BYTE);
  Serial2.write((uint8_t *)&pkt, sizeof(GaugePacket));
  Serial2.write(END_BYTE);
}

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  Serial.print("Received from ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // Validate packet size
  if (len != sizeof(GaugePacket)) {
    Serial.print("Bad packet size: ");
    Serial.println(len);
    return;
  }

  // Copy into persistent struct
  memcpy(&currentPacket, data, sizeof(GaugePacket));
  newPacket = true;

  Serial.printf("S3 sizeof(GaugePacket) = %u\n", sizeof(GaugePacket));
  // Print the full structured packet
  // printGaugePacket(currentPacket);

  // Forward to P4 over UART
  forwardToP4(currentPacket);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("ESP-NOW Receiver Booting...");

  // Initialize UART1 for forwarding to P4
  Serial2.begin(921600, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("UART1 ready for forwarding");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Must match transmitter
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  // REQUIRED for IDF 5.1+ (Arduino 3.4.x)
  esp_wifi_start();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  Serial.println("ESP-NOW init OK");

  esp_now_register_recv_cb(onReceive);

  Serial.println("Receiver Ready");
}

void loop() {
  // Nothing needed here — LVGL task will read currentPacket
}