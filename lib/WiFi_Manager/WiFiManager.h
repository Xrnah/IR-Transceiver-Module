#pragma once

#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include "WiFiData.h" 

namespace CustomWiFi {

  enum class WiFiState {
    IDLE,
    DISCONNECTED,
    CONNECTING_SAVED,
    CONNECTING_HIDDEN,
    START_SCAN,      
    SCANNING,         
    CONNECTING_SCANNED,
    CONNECTED,
    CONNECTION_FAILED
  };

  class WiFiManager {
  public:
    WiFiManager();
    void begin();
    void begin(const char* ssid, const char* pass);
    void handleConnection();
    bool connectToHidden(const char* ssid, const char* pass);

  private:
    // --- Configuration ---
    static constexpr size_t SSID_MAX_LEN = 32;
    static constexpr size_t PASS_MAX_LEN = 64;
    
    static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;
    static constexpr unsigned long WIFI_RETRY_DELAY_MS = 2000;
    static constexpr unsigned long WIFI_CHECK_INTERVAL_MS = 30000; // Keep-alive check only

    struct StoredCredential {
      uint32_t magic;
      char ssid[SSID_MAX_LEN];
      char password[PASS_MAX_LEN];
    };
    static constexpr uint32_t EEPROM_MAGIC = 0xC0FFEE27;

    // --- State ---
    WiFiState currentState;
    unsigned long lastWiFiCheck = 0;
    unsigned long lastAttemptTime = 0;
    int retryCount = 0;
    
    char hidden_ssid[SSID_MAX_LEN] = {0};
    char hidden_pass[PASS_MAX_LEN] = {0};

    // --- Core Logic ---
    void trySavedCredentials();
    void startConnection(const char* ssid, const char* password, WiFiState nextState);
    void checkConnectionProgress();
    
    // Split scan into two phases for non-blocking operation
    void startScan();
    void handleScanResult();
    
    void handleRetry();
    void saveWiFiToEEPROM(const char* ssid, const char* password);
    bool readWiFiFromEEPROM(char* ssid, char* password);
  };
}