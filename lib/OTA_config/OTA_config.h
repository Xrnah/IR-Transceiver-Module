/*
 * OTA_config.h
 * 
 * Provides functions to manage Wi-Fi connection and Over-The-Air (OTA) updates
 * for ESP8266-based devices.
 * 
 * Features:
 * - Connects the device to a Wi-Fi network using credentials defined in OTA_setting.h.
 * - Sets up ArduinoOTA with hostname and password for secure OTA firmware updates.
 * - Provides event handlers for OTA start, completion, and error reporting via Serial.
 * 
 * Requirements:
 * - OTA_setting.h must define the OTAConfig namespace containing:
 *   - SSID: Wi-Fi network name
 *   - PASSWORD: Wi-Fi network password
 *   - HOSTNAME: OTA device hostname
 *   - OTA_PASS: OTA password for authentication
 * 
 * Usage:
 * 1. Call setupOTA() during device initialization to connect Wi-Fi and enable OTA.
 * 2. Ensure ArduinoOTA.handle() is called regularly (typically inside the loop).
 */

#pragma once

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "OTA_setting.h"  // Contains OTAConfig namespace with Wi-Fi and OTA credentials

// ─────────────────────────────────────────────
// 🌐 OTA Update State Flag
// ─────────────────────────────────────────────
bool otaInProgress = false;  // Global flag to track OTA state

// ─────────────────────────────────────────────
// 📡 Initialize Wi-Fi Connection
// ─────────────────────────────────────────────
void connectToWiFi() {
  WiFi.begin(OTAConfig::SSID, OTAConfig::PASSWORD);  // Start Wi-Fi connection

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {  // Wait until connected
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected.");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());  // Print the device's IP address
}

// ─────────────────────────────────────────────
// 📤 Setup OTA (Over-The-Air Updates)
// ─────────────────────────────────────────────
void setupOTA() {
  connectToWiFi();  // Connect to Wi-Fi first

  // Configure OTA hostname and authentication password
  ArduinoOTA.setHostname(OTAConfig::HOSTNAME);
  ArduinoOTA.setPassword(OTAConfig::OTA_PASS);

  // OTA start callback
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    Serial.println("🔄 OTA Update Start");
  });

  // OTA end callback
  ArduinoOTA.onEnd([]() {
    otaInProgress = false;
    Serial.println("✅ OTA Update Complete");
  });

  // OTA error callback
  ArduinoOTA.onError([](ota_error_t error) {
    otaInProgress = false;  // Reset OTA state on error
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

  ArduinoOTA.begin();  // Start OTA service
}
