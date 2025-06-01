#pragma once

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

// ─────────────────────────────────────────────────────────────
// 🔧 OTA Configuration Settings (can be moved to separate file)
// ─────────────────────────────────────────────────────────────
namespace OTAConfig {
  constexpr const char* SSID       = "Test_SSID";
  constexpr const char* PASSWORD   = "Test_Pass";
  constexpr const char* HOSTNAME   = "OTA_NAME";
  constexpr const char* OTA_PASS   = "Password!";
}

// ─────────────────────────────────────────────
// 📡 Initialize Wi-Fi Connection
// ─────────────────────────────────────────────
void connectToWiFi() {
  WiFi.begin(OTAConfig::SSID, OTAConfig::PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected.");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());
}

// ─────────────────────────────────────────────
// 📤 Setup OTA (Over-The-Air Updates)
// ─────────────────────────────────────────────
void setupOTA() {
  connectToWiFi();

  ArduinoOTA.setHostname(OTAConfig::HOSTNAME);
  ArduinoOTA.setPassword(OTAConfig::OTA_PASS);

  ArduinoOTA.onStart([]() {
    Serial.println("🔄 OTA Update Start");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("✅ OTA Update Complete");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("❌ OTA Error [%u]: ", error);
    switch (error) {
      case OTA_AUTH_ERROR:    Serial.println("Auth Failed"); break;
      case OTA_BEGIN_ERROR:   Serial.println("Begin Failed"); break;
      case OTA_CONNECT_ERROR: Serial.println("Connect Failed"); break;
      case OTA_RECEIVE_ERROR: Serial.println("Receive Failed"); break;
      case OTA_END_ERROR:     Serial.println("End Failed"); break;
      default:                Serial.println("Unknown Error"); break;
    }
  });

  ArduinoOTA.begin();
}
