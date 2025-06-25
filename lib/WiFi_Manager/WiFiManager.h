// WiFiManager.h
#pragma once

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <vector>

#define EEPROM_SIZE 96
#define WIFI_RETRY_DELAY 1000
#define WIFI_RETRY_LIMIT 0

extern std::vector<String> ssidBlacklist;
extern std::vector<String> ssidSkiplist;

extern const char* ramsWifiPass2024;
extern const char* ramsPcPass2024;

struct WiFiCredential {
  const char* ssid;
  const char* password;
};

extern WiFiCredential wifiTable[];
extern const int WIFI_COUNT;

extern unsigned long lastWiFiCheck;
const unsigned long wifiCheckInterval = 10000;

bool isBlacklisted(const String& ssid);
bool isInList(const std::vector<String>& list, const String& ssid);
void saveWiFiToEEPROM(const char* ssid, const char* password);
bool readWiFiFromEEPROM(char* ssid, char* password);
bool connectToWiFi(const char* ssid, const char* password);
bool autoConnectWiFi();
bool autoConnectWiFi(const char* hardcoded_ssid, const char* hardcoded_pass);
void autoConnectWiFiWithRetry();
void checkWiFi();
