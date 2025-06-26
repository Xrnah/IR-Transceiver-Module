// WiFiManager.cpp

#include "WiFiManager.h"

const char* ramsWifiPass2024 = "#Ramswifi";
const char* ramsPcPass2024 = "bearam";

WiFiCredential wifiTable[] = {

// Test and Development
{"7/11", "Baliwkaba?"},
{"Keanu Geronimo", "AnongPassword?"},
{"Mansion 103", "Connection_503"},
{"epic_vivo", "epicgamer"},

// Basement 3 - Engineering Experiment and Testing Floor
// Basement 2 - Parking B2
// Basement 1 - Parking B1

// Ground Floor - Administration / MPH1 / Cafeteria
{"WiFi-Cafeteria", ramsWifiPass2024},
// 2nd Floor - SHS / MS Room
// 3rd Floor - SHS / SAO
// 4th Floor - Full Time Faculty / Registrar 
// 5th Floor - School of Information Technology
// 6th Floor - School of Psychology
{"WiFi-609(A)", ramsWifiPass2024},
{"WiFi-609(B)", ramsWifiPass2024},
{"WiFi-609(C)", ramsWifiPass2024},
// 7th Floor - Library
{"WiFi-7F", ramsWifiPass2024},
{"WiFi-Library", ramsWifiPass2024},
// 8th Floor - School of Engineering
{"WiFi-8F", ramsWifiPass2024},
// * 801 - Digital Electronics Laboratory
{"WiFi-801", ramsWifiPass2024},
// * 802 - National Instrumental Laboratory
{"WiFi-802", ramsWifiPass2024},
// * 803 - ESLO Office
// Meron ba?
// * 805 - Electronics and Circuit Laboratory
{"WiFi-805", ramsWifiPass2024},
// * 806 - Physics Laboratory
{"WiFi-806", ramsWifiPass2024},
// * 807 - Networking and Communications Laboratory
{"WiFi-807", ramsWifiPass2024},
// * 808 - A.I.R HUB
{"APC-Airhub", "APC_Airlab_2023!"},
// Airhub5G
// AirhubGuest
// * 809 - Classroom
{"WiFi-809", ramsWifiPass2024},
// * 811 - Classroom
{"WiFi-811", ramsWifiPass2024},
// * 813 - Classroom
{"WiFi-813", ramsWifiPass2024},
// * 814 - Project Rooms (6)
{"WiFi-814", ramsWifiPass2024},
// * 815 - Classroom
{"WiFi-815", ramsWifiPass2024},
// * 816/818 - Hydraulic Laboratory / Soil Test Laboratory
{"WiFi-816", ramsWifiPass2024},
{"WiFi-818", ramsWifiPass2024},
// * 817 - Classroom
{"WiFi-817", ramsWifiPass2024},

// 9th Floor - Dormitory
// 10th Floor - Guidance / School of Multimedia and Arts
// 11th Floor - Gymnasium / School of Architecture
// 12th Floor - Auditorium
// 13th Roof Deck - Helipad / Auditorium Tech Room

};

const int WIFI_COUNT = sizeof(wifiTable) / sizeof(wifiTable[0]);

std::vector<String> ssidBlacklist;
std::vector<String> ssidSkiplist;

unsigned long lastWiFiCheck = 0;

bool isBlacklisted(const String& ssid) {
  for (const auto& black : ssidBlacklist) {
    if (black == ssid) return true;
  }
  return false;
}

bool isInList(const std::vector<String>& list, const String& ssid) {
  for (const auto& entry : list) {
    if (entry == ssid) return true;
  }
  return false;
}

void saveWiFiToEEPROM(const char* ssid, const char* password) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; ++i)
    EEPROM.write(i, i < strlen(ssid) ? ssid[i] : 0);
  for (int i = 0; i < 64; ++i)
    EEPROM.write(32 + i, i < strlen(password) ? password[i] : 0);
  EEPROM.commit();
}

bool readWiFiFromEEPROM(char* ssid, char* password) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; ++i)
    ssid[i] = EEPROM.read(i);
  ssid[31] = '\0';
  for (int i = 0; i < 64; ++i)
    password[i] = EEPROM.read(32 + i);
  password[63] = '\0';
  return strlen(ssid) > 0;
}

bool connectToWiFi(const char* ssid, const char* password) {
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

bool autoConnectWiFi(const char* hardcoded_ssid, const char* hardcoded_pass) {
  WiFi.mode(WIFI_STA);
  Serial.printf("📡 Trying hardcoded hidden network: %s\n", hardcoded_ssid);
  return connectToWiFi(hardcoded_ssid, hardcoded_pass);
}

bool autoConnectWiFi() {
  WiFi.mode(WIFI_STA);
  static bool triedSaved = false;
  if (!triedSaved) {
    triedSaved = true;
    char savedSSID[32], savedPass[64];
    if (readWiFiFromEEPROM(savedSSID, savedPass)) {
      Serial.printf("💾 Trying saved WiFi: %s\n", savedSSID);
      if (connectToWiFi(savedSSID, savedPass)) return true;
    }
  }

  int found = WiFi.scanNetworks();
  Serial.printf("🔍 Found %d networks\n", found);
  int bestIndex = -1;
  int bestRSSI = -1000;

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

void autoConnectWiFiWithRetry() {
  int attempt = 0;
  while (!autoConnectWiFi()) {
    attempt++;
    Serial.printf("🔁 WiFi retry attempt #%d\n", attempt);
    if (WIFI_RETRY_LIMIT > 0 && attempt >= WIFI_RETRY_LIMIT) {
      Serial.println("🛑 Retry limit reached. Halting.");
      return;
    }
    delay(WIFI_RETRY_DELAY);
  }
}

void checkWiFi() {
  if (millis() - lastWiFiCheck < wifiCheckInterval) return;
  lastWiFiCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📴 WiFi disconnected! Attempting reconnect...");
    if (autoConnectWiFi()) {
      Serial.println("🔁 WiFi reconnected successfully.");
    } else {
      Serial.println("⚠️ Failed to reconnect WiFi.");
    }
  }
}
