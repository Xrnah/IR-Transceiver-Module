/*
 * WiFiManager.h
 *
 * Refactored to a non-blocking, state-machine-based design for better
 * encapsulation, responsiveness, and maintainability.
 *
 * Features:
 * - Asynchronously handles connection logic without blocking the main loop.
 * - Supports EEPROM save/load of last successful credentials
 * - Robust reconnect logic with configurable timeouts and retries.
 *
 * Usage:
 * - Instantiate the WiFiManager class.
 * - Call begin() in setup().
 * - Call handleConnection() in the main loop().
 */

#pragma once

#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include "WiFiData.h"

namespace CustomWiFi {
    
  // Represents the current state of the WiFi connection process.
  enum class WiFiState {
    IDLE,
    CONNECTING_SAVED,
    SCANNING,
    CONNECTING_SCANNED,
    CONNECTED,
    DISCONNECTED,
    CONNECTION_FAILED
  };

  class WiFiManager {
  public:
    // --- Public API ---
    WiFiManager();
    void begin();
    void handleConnection();
    bool connectToHidden(const char* ssid, const char* pass);

  private:
    // --- Configuration ---
    static constexpr size_t SSID_MAX_LEN = 32;
    static constexpr size_t PASS_MAX_LEN = 64;
    static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;
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
    WiFiState currentState;
    unsigned long lastWiFiCheck = 0;
    unsigned long lastAttemptTime = 0;
    int retryCount = 0;

    // --- Core Logic ---
    void trySavedCredentials();
    void startConnection(const char* ssid, const char* password, WiFiState nextState);
    void checkConnectionProgress();
    void findAndConnectToBestNetwork();
    void handleRetry();
    void saveWiFiToEEPROM(const char* ssid, const char* password);
    bool readWiFiFromEEPROM(char* ssid, char* password);
  };
} // namespace CustomWiFi
