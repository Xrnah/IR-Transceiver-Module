/*
 * WiFiManager.h
 *
 * Refactored to a class-based design for better encapsulation and maintainability.
 * Handles WiFi auto-connection, scanning, and recovery for ESP8266.
 *
 * Features:
 * - Scans for known WiFi networks using a predefined table of SSID-password pairs
 * - Automatically connects to the strongest available known SSID
 * - Supports EEPROM save/load of last successful credentials
 * - Reconnect logic with retry attempt tracking
 *
 * Usage:
 * - Instantiate the WiFiManager class.
 * - Call autoConnectWithRetry() in setup().
 * - Call checkConnection() in the main loop().
 */

#pragma once

#include <ESP8266WiFi.h>
#include <EEPROM.h>

struct WiFiCredential {
  const char* ssid;
  const char* password;
};

class WiFiManager {
public:
  // --- Public API ---
  WiFiManager();
  void autoConnectWithRetry();
  void checkConnection();
  bool connectToHidden(const char* ssid, const char* pass);

private:
  // --- Configuration ---
  static constexpr size_t SSID_MAX_LEN = 32;
  static constexpr size_t PASS_MAX_LEN = 64;
  static constexpr unsigned long WIFI_RETRY_DELAY_MS = 1000;
  static constexpr int WIFI_RETRY_LIMIT = 0; // 0 for infinite retries
  static constexpr unsigned long WIFI_CHECK_INTERVAL_MS = 10000;

  struct StoredCredential {
    uint32_t magic; // To validate EEPROM data
    char ssid[SSID_MAX_LEN];
    char password[PASS_MAX_LEN];
  };
  static constexpr uint32_t EEPROM_MAGIC = 0xDEADBEEF;

  // --- State ---
  unsigned long lastWiFiCheck = 0;

  // --- Core Logic ---
  bool autoConnect();
  bool connectToWiFi(const char* ssid, const char* password);
  void saveWiFiToEEPROM(const char* ssid, const char* password);
  bool readWiFiFromEEPROM(char* ssid, char* password);
};

