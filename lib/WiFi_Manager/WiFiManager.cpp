// WiFiManager.cpp

#include "WiFiManager.h"
#include "wifi_credentials.h" // Decoupled credentials

WiFiManager::WiFiManager() {
  // Constructor can be used for one-time setup if needed.
  // For now, it's empty.
}

void WiFiManager::saveWiFiToEEPROM(const char* ssid, const char* password) {
  EEPROM.begin(sizeof(StoredCredential));
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

bool WiFiManager::readWiFiFromEEPROM(char* ssid, char* password) {
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

constexpr size_t SSID_MAX_LEN = 32;
constexpr size_t PASS_MAX_LEN = 64;

bool WiFiManager::connectToWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  Serial.printf("\n🔌 Trying to connect to WiFi: %s", ssid);
  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ WiFi connected.");
      Serial.print("📡 IP Address: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n❌ Failed to connect.");
  return false;
}

bool WiFiManager::connectToHidden(const char* hardcoded_ssid, const char* hardcoded_pass) {
  WiFi.mode(WIFI_STA);
  Serial.printf("📡 Trying hardcoded hidden network: %s\n", hardcoded_ssid);
  return connectToWiFi(hardcoded_ssid, hardcoded_pass);
}

bool WiFiManager::autoConnect() {
  WiFi.mode(WIFI_STA);
  static bool triedSaved = false;
  if (!triedSaved) {
    triedSaved = true;
    char savedSSID[SSID_MAX_LEN], savedPass[PASS_MAX_LEN];
    if (readWiFiFromEEPROM(savedSSID, savedPass)) {
      Serial.printf("💾 Trying saved WiFi: %s\n", savedSSID);
      if (connectToWiFi(savedSSID, savedPass)) return true;
    }
  }

  int found = WiFi.scanNetworks();
  Serial.printf("🔍 Found %d networks\n", found);
  int bestIndex = -1;
  int bestRSSI = -1000;

  const int WIFI_COUNT = sizeof(wifiTable) / sizeof(wifiTable[0]);

  for (int i = 0; i < found; i++) {
    String ssidScanned = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    int channel = WiFi.channel(i);
    uint8_t* bssidRaw = WiFi.BSSID(i);

    char bssidStr[18];
    sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            bssidRaw[0], bssidRaw[1], bssidRaw[2],
            bssidRaw[3], bssidRaw[4], bssidRaw[5]);

    Serial.printf("📡 SSID: %-20s RSSI: %-4d CH: %-2d BSSID: %s\n",
                  ssidScanned.c_str(), rssi, channel, bssidStr);

    bool isKnown = false;

    for (int j = 0; j < WIFI_COUNT; j++) {
      if (ssidScanned == wifiTable[j].ssid) {
        isKnown = true;
        if (rssi > bestRSSI) {
          bestIndex = j;
          bestRSSI = rssi;
        }
      }
    }

    if (!isKnown) {
      Serial.printf("⚠️  Skipping unknown SSID: %s\n", ssidScanned.c_str());
    }
  }

  if (bestIndex != -1) {
    const char* ssid = wifiTable[bestIndex].ssid;
    const char* password = wifiTable[bestIndex].password;
    Serial.printf("✅ Trying strongest known SSID: %s (RSSI: %d)\n", ssid, bestRSSI);
    if (connectToWiFi(ssid, password)) {
      saveWiFiToEEPROM(ssid, password);
      return true;
    }
  }

  Serial.println("❌ No strong matching networks succeeded.");
  return false;
}

void WiFiManager::autoConnectWithRetry() {
  int attempt = 0;
  while (!autoConnect()) {
    attempt++;
    Serial.printf("🔁 WiFi retry attempt #%d\n", attempt);
    if (WIFI_RETRY_LIMIT > 0 && attempt >= WIFI_RETRY_LIMIT) {
      Serial.println("🛑 Retry limit reached. Halting.");
      return;
    }
    delay(WIFI_RETRY_DELAY_MS);
  }
}

void WiFiManager::checkConnection() {
  if (millis() - lastWiFiCheck < WIFI_CHECK_INTERVAL_MS) return;
  lastWiFiCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📴 WiFi disconnected! Attempting reconnect...");
    if (autoConnect()) {
      Serial.println("🔁 WiFi reconnected successfully.");
    } else {
      Serial.println("⚠️ Failed to reconnect WiFi.");
    }
  }
}

