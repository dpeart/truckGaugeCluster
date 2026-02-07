#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "GaugePacket.h"   // <-- includes your struct + printGaugePacket()

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

  // Copy into your struct
  GaugePacket pkt;
  memcpy(&pkt, data, sizeof(GaugePacket));

  // Print the full structured packet
  printGaugePacket(pkt);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("ESP-NOW Receiver Booting...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // REQUIRED for IDF 5.1+ (Arduino 3.4.x)
  esp_wifi_start();

  // Must match transmitter
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  Serial.println("ESP-NOW init OK");

  esp_now_register_recv_cb(onReceive);

  Serial.println("Receiver Ready");
}

void loop() {}
