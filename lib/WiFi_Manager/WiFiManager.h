/*
 * WiFiManager.h
 *
 * Handles WiFi auto-connection, scanning, and recovery logic for ESP8266.
 * Designed to prioritize stable and flexible WiFi selection across multiple floors 
 * or environments.
 *
 * Features:
 * - Scans for known WiFi networks using a predefined table of SSID-password pairs
 * - Automatically connects to the strongest available known SSID
 * - Supports EEPROM save/load of last successful credentials
 * - Optional hardcoded fallback for hidden SSIDs (e.g., admin/testing)
 * - Reconnect logic with retry attempt tracking
 *
 * Connection Methods:
 * - autoConnectWiFi(const char* ssid, const char* pass)
 *     → Connects to a hardcoded hidden/test network without scanning.
 * - autoConnectWiFi()
 *     → Scans nearby networks and matches against known WiFi table entries.
 *       If successful, credentials are stored in EEPROM for future boots.
 * - autoConnectWiFiWithRetry()
 *     → Calls autoConnectWiFi() with retry delay and optional retry limit.
 *
 * Usage:
 * - Call a connection method in setup() to establish WiFi on boot
 * - Call checkWiFi() periodically in loop() to ensure reconnection
 * - Customize the wifiTable[] to define known access points
 */

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
