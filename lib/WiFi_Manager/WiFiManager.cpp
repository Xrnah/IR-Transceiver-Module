// WiFiManager.cpp

#include "WiFiManager.h"
#include "logging.h"

namespace {
constexpr const char* k_log_tag = "WIFI";

void formatIpAddress(char* buf, size_t len, const IPAddress& ip) {
  if (buf == nullptr || len == 0) return;
  snprintf(buf, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}
} // namespace

CustomWiFi::WiFiManager::WiFiManager()
    : current_state(CustomWiFi::WiFiState::IDLE),
      last_attempt_time(0),
      retry_count(0) {}

void CustomWiFi::WiFiManager::saveWiFiToEEPROM(const char* ssid, const char* password) {
  EEPROM.begin(sizeof(StoredCredential));
  StoredCredential creds;
  // Read first to avoid unnecessary writes (flash wear leveling)
  EEPROM.get(0, creds);
  if (creds.magic == eeprom_magic && 
      strncmp(creds.ssid, ssid, ssid_max_len) == 0 && 
      strncmp(creds.password, password, pass_max_len) == 0) {
      EEPROM.end();
      return; 
  }
  
  creds.magic = eeprom_magic;
  strncpy(creds.ssid, ssid, ssid_max_len - 1);
  strncpy(creds.password, password, pass_max_len - 1);
  EEPROM.put(0, creds);
  EEPROM.commit();
  EEPROM.end();
}

bool CustomWiFi::WiFiManager::readWiFiFromEEPROM(char* ssid, char* password) {
  EEPROM.begin(sizeof(StoredCredential));
  StoredCredential creds;
  EEPROM.get(0, creds);
  EEPROM.end();

  if (creds.magic == eeprom_magic && strlen(creds.ssid) > 0) {
    strncpy(ssid, creds.ssid, ssid_max_len);
    strncpy(password, creds.password, pass_max_len);
    return true;
  }
  return false;
}

void CustomWiFi::WiFiManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  if (WiFi.status() != WL_CONNECTED) {
    current_state = CustomWiFi::WiFiState::DISCONNECTED;
  }
}

void CustomWiFi::WiFiManager::begin(const char* ssid, const char* pass) {
  strncpy(hidden_ssid, ssid ? ssid : "", sizeof(hidden_ssid) - 1);
  hidden_ssid[sizeof(hidden_ssid) - 1] = '\0';
  strncpy(hidden_pass, pass ? pass : "", sizeof(hidden_pass) - 1);
  hidden_pass[sizeof(hidden_pass) - 1] = '\0';

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  current_state = CustomWiFi::WiFiState::DISCONNECTED;
}

void CustomWiFi::WiFiManager::handleConnection() {
  
  switch (current_state) {
    case CustomWiFi::WiFiState::IDLE:
      break;

    case CustomWiFi::WiFiState::CONNECTED:
      if (millis() - last_wifi_check > wifi_check_interval_ms) {
        last_wifi_check = millis();
        if (WiFi.status() != WL_CONNECTED) {
          logWarn(k_log_tag, "WiFi disconnected! Attempting reconnect...");
          current_state = CustomWiFi::WiFiState::DISCONNECTED;
          retry_count = 0;
        }
      }
      break;

    case CustomWiFi::WiFiState::DISCONNECTED:
      logInfo(k_log_tag, "Starting connection process...");
      if (strlen(hidden_ssid) > 0) {
        startConnection(hidden_ssid, hidden_pass, CustomWiFi::WiFiState::CONNECTING_HIDDEN);
      } else {
        trySavedCredentials();
      }
      break;

    case CustomWiFi::WiFiState::CONNECTING_SAVED:
    case CustomWiFi::WiFiState::CONNECTING_SCANNED:
    case CustomWiFi::WiFiState::CONNECTING_HIDDEN:
      checkConnectionProgress();
      break;

    case CustomWiFi::WiFiState::START_SCAN:
      startScan();
      break;

    case CustomWiFi::WiFiState::SCANNING:
      handleScanResult();
      break;

    case CustomWiFi::WiFiState::CONNECTION_FAILED:
      handleRetry();
      break;
  }
}

void CustomWiFi::WiFiManager::trySavedCredentials() {
  char saved_ssid[ssid_max_len], saved_pass[pass_max_len];
  if (readWiFiFromEEPROM(saved_ssid, saved_pass)) {
    logInfo(k_log_tag, "Trying saved WiFi: %s", saved_ssid);
    startConnection(saved_ssid, saved_pass, CustomWiFi::WiFiState::CONNECTING_SAVED);
  } else {
    logInfo(k_log_tag, "No saved credentials. Queuing scan...");
    current_state = CustomWiFi::WiFiState::START_SCAN;
  }
}

void CustomWiFi::WiFiManager::startConnection(const char* ssid, const char* password, WiFiState next_state) {
  WiFi.disconnect(); 
  yield(); // Feed WDT before intensive radio work
  WiFi.begin(ssid, password);
  logInfo(k_log_tag, "Trying to connect to WiFi: %s", ssid);
  current_state = next_state;
  last_attempt_time = millis();
}

void CustomWiFi::WiFiManager::checkConnectionProgress() {
  if (WiFi.status() == WL_CONNECTED) {
    logInfo(k_log_tag, "WiFi connected.");
    char ip_buffer[16];
    formatIpAddress(ip_buffer, sizeof(ip_buffer), WiFi.localIP());
    logInfo(k_log_tag, "IP Address: %s", ip_buffer);

    // Only save if we connected via a dynamic method (Scan or Hidden manual override)
    if (current_state == CustomWiFi::WiFiState::CONNECTING_SCANNED || 
        current_state == CustomWiFi::WiFiState::CONNECTING_HIDDEN) {
      logInfo(k_log_tag, "Saving successful credentials to EEPROM...");
      saveWiFiToEEPROM(WiFi.SSID().c_str(), WiFi.psk().c_str());
    }

    current_state = CustomWiFi::WiFiState::CONNECTED;
    retry_count = 0;
    return;
  }

  if (millis() - last_attempt_time > wifi_connect_timeout_ms) {
    logWarn(k_log_tag, "Connection attempt timed out.");
    WiFi.disconnect();
    yield();

    if (current_state == CustomWiFi::WiFiState::CONNECTING_SAVED) {
      // If saved creds failed, try scanning
      current_state = CustomWiFi::WiFiState::START_SCAN;
    } else {
      current_state = CustomWiFi::WiFiState::CONNECTION_FAILED;
    }
  }
}

// Deprecated (012226): connectToHidden() is implemented as an overload of begin().
bool CustomWiFi::WiFiManager::connectToHidden(const char* ssid, const char* pass) {
  strncpy(hidden_ssid, ssid ? ssid : "", sizeof(hidden_ssid) - 1);
  hidden_ssid[sizeof(hidden_ssid) - 1] = '\0'; // Ensure null-termination
  strncpy(hidden_pass, pass ? pass : "", sizeof(hidden_pass) - 1);
  hidden_pass[sizeof(hidden_pass) - 1] = '\0'; // Ensure null-termination
  WiFi.mode(WIFI_STA);
  current_state = CustomWiFi::WiFiState::DISCONNECTED;
  return true;
}

void CustomWiFi::WiFiManager::startScan() {
  logInfo(k_log_tag, "Starting async WiFi scan...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  yield();
  WiFi.scanNetworks(true); // 'true' = ASYNC MODE. Returns immediately.
  current_state = CustomWiFi::WiFiState::SCANNING;
}

void CustomWiFi::WiFiManager::handleScanResult() {
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) return; // Still scanning, do nothing

  if (n < 0) {
    logError(k_log_tag, "Scan failed.");
    current_state = CustomWiFi::WiFiState::CONNECTION_FAILED;
    return;
  }

  logInfo(k_log_tag, "Found %d networks.", n);
  
  int best_index = -1;
  int best_rssi = -1000;

  // Search for known networks in the scan results
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < k_wifi_count; j++) {
      if (strcmp(WiFi.SSID(i).c_str(), k_wifi_table[j].ssid) == 0) {
        if (WiFi.RSSI(i) > best_rssi) {
          best_rssi = WiFi.RSSI(i);
          best_index = j;
        }
      }
    }
    yield();
  }

  WiFi.scanDelete(); // Clean up RAM

  if (best_index != -1) {
    const char* ssid = k_wifi_table[best_index].ssid;
    const char* password = k_wifi_table[best_index].password;
    logInfo(k_log_tag, "Found strongest known SSID: %s (%d dBm)", ssid, best_rssi);
    startConnection(ssid, password, CustomWiFi::WiFiState::CONNECTING_SCANNED);
  } else {
    logWarn(k_log_tag, "No known networks found.");
    current_state = CustomWiFi::WiFiState::CONNECTION_FAILED;
  }
}

void CustomWiFi::WiFiManager::handleRetry() {
  if (millis() - last_attempt_time > wifi_retry_delay_ms) {
    retry_count++;
    if (retry_count > 10) { 
        logWarn(k_log_tag, "Too many retries. Pausing...");
        last_attempt_time = millis() + 60000; // Wait 1 min
        retry_count = 0;
        return;
    }
    
    logInfo(k_log_tag, "Retry attempt #%d", retry_count);
    current_state = CustomWiFi::WiFiState::DISCONNECTED;
    last_attempt_time = millis();
  }
}
