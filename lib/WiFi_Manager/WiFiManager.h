#pragma once

/*
 * WiFiManager.h
 *
 * Non-blocking Wi-Fi connection manager for ESP8266.
 */

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
    /**
     * @brief Construct a new WiFiManager instance.
     */
    WiFiManager();

    /**
     * @brief Initialize Wi-Fi state (no credentials provided).
     */
    void begin();

    /**
     * @brief Initialize Wi-Fi state with hidden SSID credentials.
     *
     * @param ssid Hidden SSID name.
     * @param pass Hidden SSID password.
     */
    void begin(const char* ssid, const char* pass);

    /**
     * @brief Run the connection state machine.
     */
    void handleConnection();

    /**
     * @brief Provide hidden SSID credentials.
     *
     * @param ssid Hidden SSID name.
     * @param pass Hidden SSID password.
     * @return true if the credentials were accepted.
     */
    bool connectToHidden(const char* ssid, const char* pass);

  private:
    // --- Configuration ---
    static constexpr size_t ssid_max_len = 32;
    static constexpr size_t pass_max_len = 64;
    
    static constexpr unsigned long wifi_connect_timeout_ms = 10000;
    static constexpr unsigned long wifi_retry_delay_ms = 2000;
    static constexpr unsigned long wifi_check_interval_ms = 30000; // Keep-alive check only

    struct StoredCredential {
      uint32_t magic;
      char ssid[ssid_max_len];
      char password[pass_max_len];
    };
    static constexpr uint32_t eeprom_magic = 0xC0FFEE27;

    // --- State ---
    WiFiState current_state;
    unsigned long last_wifi_check = 0;
    unsigned long last_attempt_time = 0;
    int retry_count = 0;
    
    char hidden_ssid[ssid_max_len] = {0};
    char hidden_pass[pass_max_len] = {0};

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
