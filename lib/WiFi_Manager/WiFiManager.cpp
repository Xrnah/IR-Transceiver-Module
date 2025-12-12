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
      trySavedCredentials();
      break;

    case CustomWiFi::WiFiState::CONNECTING_SAVED:
    case CustomWiFi::WiFiState::CONNECTING_SCANNED:
      checkConnectionProgress();
      break;

    case WiFiState::SCANNING:
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
  // Store the state before it's updated on success
  WiFiState previousState = currentState;

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected.");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    currentState = CustomWiFi::WiFiState::CONNECTED;
    retryCount = 0;  // Reset on success
    // If we connected via a scan, save the successful credentials
    if (previousState == CustomWiFi::WiFiState::CONNECTING_SCANNED) {
      saveWiFiToEEPROM(WiFi.SSID().c_str(), WiFi.psk().c_str());
    }
    return;
  }

  // Check for connection timeout
  if (millis() - lastAttemptTime > WIFI_CONNECT_TIMEOUT_MS) {
    Serial.println("\n❌ Connection attempt timed out.");
    WiFi.disconnect();

    // If connecting with saved credentials failed, try scanning next
    if (currentState == CustomWiFi::WiFiState::CONNECTING_SAVED) {
      Serial.println("Saved credentials failed. Scanning for networks...");
      currentState = CustomWiFi::WiFiState::SCANNING;
    } else {
      currentState = CustomWiFi::WiFiState::CONNECTION_FAILED;
    }
  }
}

bool CustomWiFi::WiFiManager::connectToHidden(const char* hardcoded_ssid, const char* hardcoded_pass) {
  int hidden_retry_count = 0;
  WiFi.mode(WIFI_STA);

  while (WiFi.status() != WL_CONNECTED) {
    hidden_retry_count++;
    Serial.printf("\n[HIDDEN] Attempt #%d to connect to %s...\n", hidden_retry_count, hardcoded_ssid);

    WiFi.begin(hardcoded_ssid, hardcoded_pass);

    unsigned long attempt_start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - attempt_start < WIFI_CONNECT_TIMEOUT_MS)) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ WiFi connected to hidden network.");
      Serial.print("📡 IP Address: ");
      Serial.println(WiFi.localIP());
      currentState = CustomWiFi::WiFiState::CONNECTED;
      return true;
    } else {
      Serial.printf("\n❌ Connection to hidden network failed. Retrying in %lu ms...\n", WIFI_RETRY_DELAY_MS);
      delay(WIFI_RETRY_DELAY_MS);
    }
  }
  return false; // Should not be reached if connection is successful
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
