#ifndef ESP32DEAUTHER_H
#define ESP32DEAUTHER_H

#include <Arduino.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <M5Cardputer.h>
#include "types.h"

#define DEAUTH_TYPE_SINGLE 1
#define DEAUTH_TYPE_ALL 2
#define CHANNEL_MAX 13
#define NUM_FRAMES_PER_DEAUTH 10
#define AP_SSID "CardPuterAP"
#define AP_PASS "password"

class ESP32Deauther {
public:
    ESP32Deauther();
    bool begin();
    bool startDeauthNetwork(const String& bssid, int channel, uint16_t reason = 7);
    bool startDeauthAll(uint16_t reason = 7);
    void stopDeauth();
    void handleDeauth();
    bool isDeauthRunning() const { return deauthRunning; }
    bool deauthNetwork(const String& bssid, int channel, uint16_t reason = 7);
    uint32_t getPacketCount() const { return packetCount; }
    String getLastError() const { return lastError; }

private:
    deauth_frame_t deauthFrame;
    bool deauthRunning;
    int deauthType;
    String targetBSSID;
    int currentChannel;
    uint32_t packetCount;
    bool key9PressedLast;
    unsigned long lastDeauthTime;
    String lastError;

    static ESP32Deauther* instance;

    bool parseMacAddress(const String& macStr, uint8_t* mac);
    void fillDeauthFrame(uint8_t* dest, uint8_t* src, uint8_t* bssid, uint16_t reason);
    bool sendDeauthFrames(wifi_interface_t ifx, int count);
    bool setChannel(int channel);
    bool checkInterface(wifi_interface_t ifx);
};

#endif // ESP32DEAUTHER_H