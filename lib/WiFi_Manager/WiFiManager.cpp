// WiFiManager.cpp

#include "WiFiManager.h"

CustomWiFi::WiFiManager::WiFiManager()
    : currentState(CustomWiFi::WiFiState::IDLE),
      lastAttemptTime(0),
      retryCount(0) {}

void CustomWiFi::WiFiManager::saveWiFiToEEPROM(const char* ssid, const char* password) {
  EEPROM.begin(sizeof(StoredCredential));
  StoredCredential creds;
  // Read first to avoid unnecessary writes (flash wear leveling)
  EEPROM.get(0, creds);
  if (creds.magic == EEPROM_MAGIC && 
      strncmp(creds.ssid, ssid, SSID_MAX_LEN) == 0 && 
      strncmp(creds.password, password, PASS_MAX_LEN) == 0) {
      EEPROM.end();
      return; 
  }
  
  creds.magic = EEPROM_MAGIC;
  strncpy(creds.ssid, ssid, SSID_MAX_LEN - 1);
  strncpy(creds.password, password, PASS_MAX_LEN - 1);
  EEPROM.put(0, creds);
  EEPROM.commit();
  EEPROM.end();
}

bool CustomWiFi::WiFiManager::readWiFiFromEEPROM(char* ssid, char* password) {
  EEPROM.begin(sizeof(StoredCredential));
  StoredCredential creds;
  EEPROM.get(0, creds);
  EEPROM.end();

  if (creds.magic == EEPROM_MAGIC && strlen(creds.ssid) > 0) {
    strncpy(ssid, creds.ssid, SSID_MAX_LEN);
    strncpy(password, creds.password, PASS_MAX_LEN);
    return true;
  }
  return false;
}

void CustomWiFi::WiFiManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  if (WiFi.status() != WL_CONNECTED) {
    currentState = CustomWiFi::WiFiState::DISCONNECTED;
  }
}

void CustomWiFi::WiFiManager::begin(const char* ssid, const char* pass) {
  strncpy(hidden_ssid, ssid ? ssid : "", sizeof(hidden_ssid) - 1);
  hidden_ssid[sizeof(hidden_ssid) - 1] = '\0';
  strncpy(hidden_pass, pass ? pass : "", sizeof(hidden_pass) - 1);
  hidden_pass[sizeof(hidden_pass) - 1] = '\0';

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  currentState = CustomWiFi::WiFiState::DISCONNECTED;
}

void CustomWiFi::WiFiManager::handleConnection() {
  
  switch (currentState) {
    case CustomWiFi::WiFiState::IDLE:
      break;

    case CustomWiFi::WiFiState::CONNECTED:
      if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL_MS) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("📴 WiFi disconnected! Attempting reconnect...");
          currentState = CustomWiFi::WiFiState::DISCONNECTED;
          retryCount = 0;
        }
      }
      break;

    case CustomWiFi::WiFiState::DISCONNECTED:
      Serial.println("🔁 Starting connection process...");
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
  char savedSSID[SSID_MAX_LEN], savedPass[PASS_MAX_LEN];
  if (readWiFiFromEEPROM(savedSSID, savedPass)) {
    Serial.printf("💾 Trying saved WiFi: %s\n", savedSSID);
    startConnection(savedSSID, savedPass, CustomWiFi::WiFiState::CONNECTING_SAVED);
  } else {
    Serial.println("No saved credentials. Queuing scan...");
    currentState = CustomWiFi::WiFiState::START_SCAN;
  }
}

void CustomWiFi::WiFiManager::startConnection(const char* ssid, const char* password, WiFiState nextState) {
  WiFi.disconnect(); 
  yield(); // Feed WDT before intensive radio work
  WiFi.begin(ssid, password);
  Serial.printf("\n🔌 Trying to connect to WiFi: %s\n", ssid);
  currentState = nextState;
  lastAttemptTime = millis();
}

void CustomWiFi::WiFiManager::checkConnectionProgress() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected.");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());

    // Only save if we connected via a dynamic method (Scan or Hidden manual override)
    if (currentState == CustomWiFi::WiFiState::CONNECTING_SCANNED || 
        currentState == CustomWiFi::WiFiState::CONNECTING_HIDDEN) {
      Serial.println("💾 Saving successful credentials to EEPROM...");
      saveWiFiToEEPROM(WiFi.SSID().c_str(), WiFi.psk().c_str());
    }

    currentState = CustomWiFi::WiFiState::CONNECTED;
    retryCount = 0;
    return;
  }

  if (millis() - lastAttemptTime > WIFI_CONNECT_TIMEOUT_MS) {
    Serial.println("\n❌ Connection attempt timed out.");
    WiFi.disconnect();
    yield();

    if (currentState == CustomWiFi::WiFiState::CONNECTING_SAVED) {
      // If saved creds failed, try scanning
      currentState = CustomWiFi::WiFiState::START_SCAN;
    } else {
      currentState = CustomWiFi::WiFiState::CONNECTION_FAILED;
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
  currentState = CustomWiFi::WiFiState::DISCONNECTED;
  return true;
}

void CustomWiFi::WiFiManager::startScan() {
  Serial.println("🔍 Starting async WiFi scan...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  yield();
  WiFi.scanNetworks(true); // 'true' = ASYNC MODE. Returns immediately.
  currentState = CustomWiFi::WiFiState::SCANNING;
}

void CustomWiFi::WiFiManager::handleScanResult() {
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) return; // Still scanning, do nothing

  if (n < 0) {
    Serial.println("❌ Scan failed.");
    currentState = CustomWiFi::WiFiState::CONNECTION_FAILED;
    return;
  }

  Serial.printf("🔍 Found %d networks.\n", n);
  
  int bestIndex = -1;
  int bestRSSI = -1000;

  // Search for known networks in the scan results
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < WIFI_COUNT; j++) {
      if (strcmp(WiFi.SSID(i).c_str(), wifiTable[j].ssid) == 0) {
        if (WiFi.RSSI(i) > bestRSSI) {
          bestRSSI = WiFi.RSSI(i);
          bestIndex = j;
        }
      }
    }
    yield();
  }

  WiFi.scanDelete(); // Clean up RAM

  if (bestIndex != -1) {
    const char* ssid = wifiTable[bestIndex].ssid;
    const char* password = wifiTable[bestIndex].password;
    Serial.printf("✅ Found strongest known SSID: %s (%d dBm)\n", ssid, bestRSSI);
    startConnection(ssid, password, CustomWiFi::WiFiState::CONNECTING_SCANNED);
  } else {
    Serial.println("❌ No known networks found.");
    currentState = CustomWiFi::WiFiState::CONNECTION_FAILED;
  }
}

void CustomWiFi::WiFiManager::handleRetry() {
  if (millis() - lastAttemptTime > WIFI_RETRY_DELAY_MS) {
    retryCount++;
    if (retryCount > 10) { 
        Serial.println("🛑 Too many retries. Pausing...");
        lastAttemptTime = millis() + 60000; // Wait 1 min
        retryCount = 0;
        return;
    }
    
    Serial.printf("🔁 Retry attempt #%d\n", retryCount);
    currentState = CustomWiFi::WiFiState::DISCONNECTED;
    lastAttemptTime = millis();
  }
}