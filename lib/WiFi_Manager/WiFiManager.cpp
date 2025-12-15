// WiFiManager.cpp

#include "WiFiManager.h"

CustomWiFi::WiFiManager::WiFiManager()
    : currentState(CustomWiFi::WiFiState::IDLE),
      lastAttemptTime(0),
      retryCount(0) {
  // Constructor initializes state
}

void CustomWiFi::WiFiManager::saveWiFiToEEPROM(const char* ssid, const char* password) {
  EEPROM.begin(sizeof(CustomWiFi::WiFiManager::StoredCredential));
  StoredCredential creds;
  creds.magic = EEPROM_MAGIC;
  strncpy(creds.ssid, ssid, SSID_MAX_LEN);
  strncpy(creds.password, password, PASS_MAX_LEN);

  // Ensure null termination
  creds.ssid[SSID_MAX_LEN - 1] = '\0';
  creds.password[PASS_MAX_LEN - 1] = '\0';

  EEPROM.put(0, creds);
  EEPROM.commit();
  EEPROM.end();
}

bool CustomWiFi::WiFiManager::readWiFiFromEEPROM(char* ssid, char* password) {
  EEPROM.begin(sizeof(CustomWiFi::WiFiManager::StoredCredential));
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
  // Only start the connection process if we are not already connected.
  // This prevents overriding the state if connectToHidden() was successful.
  if (WiFi.status() != WL_CONNECTED) {
    currentState = CustomWiFi::WiFiState::DISCONNECTED;  // Start the process
  }
}

void CustomWiFi::WiFiManager::handleConnection() {
  // Only check connection status periodically to avoid blocking
  if (millis() - lastWiFiCheck < WIFI_CHECK_INTERVAL_MS) {
    return;
  }
  lastWiFiCheck = millis();

  // Main state machine logic
  switch (currentState) {
    case CustomWiFi::WiFiState::IDLE:
      // Do nothing until explicitly started
      break;

    case CustomWiFi::WiFiState::CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("📴 WiFi disconnected! Attempting reconnect...");
        currentState = CustomWiFi::WiFiState::DISCONNECTED;
        retryCount = 0;  // Reset retries
      }
      break;

    case CustomWiFi::WiFiState::DISCONNECTED:
      Serial.println("🔁 Starting connection process...");
      // If a hidden network is configured, prioritize it.
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

    case CustomWiFi::WiFiState::SCANNING:
      // In a real non-blocking implementation, scanning would also be managed.
      // For simplicity, we'll keep the scan blocking but integrated into the state flow.
      findAndConnectToBestNetwork();
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
    Serial.println("No saved credentials. Scanning for networks...");
    currentState = CustomWiFi::WiFiState::SCANNING;  // Move to scanning state
  }
}

void CustomWiFi::WiFiManager::startConnection(const char* ssid, const char* password, WiFiState nextState) {
  WiFi.begin(ssid, password);
  Serial.printf("\n🔌 Trying to connect to WiFi: %s\n", ssid);
  currentState = nextState;
  lastAttemptTime = millis();  // Start connection timer
}

void CustomWiFi::WiFiManager::checkConnectionProgress() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected.");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());

    // Save credentials on first successful connection from scan or hidden
    if (currentState == CustomWiFi::WiFiState::CONNECTING_SCANNED || currentState == CustomWiFi::WiFiState::CONNECTING_HIDDEN) {
      Serial.println("💾 Saving successful credentials to EEPROM...");
      saveWiFiToEEPROM(WiFi.SSID().c_str(), WiFi.psk().c_str());
    }

    currentState = CustomWiFi::WiFiState::CONNECTED;
    retryCount = 0;  // Reset on success
    return;
  }

  // Check for connection timeout
  if (millis() - lastAttemptTime > WIFI_CONNECT_TIMEOUT_MS) {
    Serial.println("\n❌ Connection attempt timed out.");
    WiFi.disconnect(true); // Disconnect and erase SDK config

    // Determine next state based on which connection attempt failed
    switch (currentState) {
      case CustomWiFi::WiFiState::CONNECTING_SAVED:
        Serial.println("Saved credentials failed. Scanning for networks...");
        currentState = CustomWiFi::WiFiState::SCANNING;
        break;
      case CustomWiFi::WiFiState::CONNECTING_HIDDEN:
      case CustomWiFi::WiFiState::CONNECTING_SCANNED:
      default:
        // For hidden, scanned, or any other state, move to the generic failure state
        currentState = CustomWiFi::WiFiState::CONNECTION_FAILED;
        break;
    }
  }
}

bool CustomWiFi::WiFiManager::connectToHidden(const char* ssid, const char* pass) {
  // Configure the manager to operate in "hidden only" mode.
  // If ssid is null or empty, this will clear the hidden config,
  // causing the manager to fall back to saved/scan mode.
  strncpy(hidden_ssid, ssid ? ssid : "", SSID_MAX_LEN - 1);
  strncpy(hidden_pass, pass ? pass : "", PASS_MAX_LEN - 1);

  WiFi.mode(WIFI_STA);
  currentState = CustomWiFi::WiFiState::DISCONNECTED; // Kick off the connection process
  return true; // Indicates that the process has been initiated.
}

void CustomWiFi::WiFiManager::findAndConnectToBestNetwork() {
  int found = WiFi.scanNetworks();
  Serial.printf("🔍 Found %d networks\n", found);
  int bestIndex = -1;
  int bestRSSI = -1000;

  if (found > 0 && WIFI_COUNT > 0) {
    for (int j = 0; j < WIFI_COUNT; j++) {
      for (int i = 0; i < found; i++) {
        if (WiFi.SSID(i) == wifiTable[j].ssid) {
          Serial.printf("✅ Found known network: %s (RSSI: %d)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
          if (WiFi.RSSI(i) > bestRSSI) {
            bestRSSI = WiFi.RSSI(i);
            bestIndex = j;
          }
        }
      }
    }
  }

  if (bestIndex != -1) {
    const char* ssid = wifiTable[bestIndex].ssid;
    const char* password = wifiTable[bestIndex].password;
    Serial.printf("✅ Trying strongest known SSID: %s (RSSI: %d)\n", ssid, bestRSSI);
    startConnection(ssid, password, CustomWiFi::WiFiState::CONNECTING_SCANNED);
  } else {
    Serial.println("❌ No known networks found in scan.");
    currentState = CustomWiFi::WiFiState::CONNECTION_FAILED;
  }
}

void CustomWiFi::WiFiManager::handleRetry() {
  if (WIFI_RETRY_LIMIT > 0 && retryCount >= WIFI_RETRY_LIMIT) {
    Serial.println("🛑 Retry limit reached. Halting connection attempts.");
    currentState = CustomWiFi::WiFiState::IDLE;  // Stop trying
    return;
  }

  if (millis() - lastAttemptTime > WIFI_RETRY_DELAY_MS) {
    retryCount++;
    Serial.printf("🔁 WiFi retry attempt #%d\n", retryCount);
    currentState = CustomWiFi::WiFiState::DISCONNECTED;  // Restart the whole process
    lastAttemptTime = millis();
  }
}
