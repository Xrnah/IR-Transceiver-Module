#include <Arduino.h>

#include "WiFiManager.h"           // Auto Wi-Fi connection manager
#include "OTA_config.h"            // OTA setup and event handlers
#include "ACU_remote_encoder.h"    // IR command generator (ACU signature)
#include "ACU_IR_modulator.h"      // Converts command to IR waveform
#include "MQTT.h"                  // MQTT messaging (PubSubClient wrapper)

// ─────────────────────────────────────────────
// 📡 Configuration
// ─────────────────────────────────────────────
#define ACUsignature   "MITSUBISHI_HEAVY_64"   // ACU IR signature
#define kIrLedPin      4                       // IR LED GPIO pin
#define rawDataLength  133                     // Raw buffer size for IR pulse timing

// Optional: Enable this to send test IR via Serial input
// #define DEBUG_IR_PRINT

// ─────────────────────────────────────────────
// 🔧 Global Objects
// ─────────────────────────────────────────────
IRsend irsend(kIrLedPin);                      // IR transmitter
uint16_t durations[rawDataLength];             // Pulse duration buffer
ACU_remote APC_ACU(ACUsignature);              // IR encoder instance
const IRProtocolConfig* selectedProtocol = &MITSUBISHI_HEAVY_64;

// ─────────────────────────────────────────────
// 🛠️ Setup (runs once on boot)
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  irsend.begin();             // IR pin setup
  delay(5000);                // Serial startup delay (skip in production)
  Serial.println("\n🔌 MCU Status: ON");

  // const char* ssid = "Test_SSID";         // Optional hardcoded credentials
  // const char* pass = "Test_Pass";
  // autoConnectWiFi(ssid, pass);             // Direct WiFi

  autoConnectWiFiWithRetry();  // Smart WiFi retry + portal
  setupOTA();                  // Start OTA service
  setupMQTTTopics();          // Build MQTT topic strings
  setupMQTT();                // Start MQTT client
}

// ─────────────────────────────────────────────
// 🔁 Main Loop
// ─────────────────────────────────────────────
void loop() {
  ArduinoOTA.handle();      // OTA process (must always run)

  checkWiFi();              // WiFi health check (runs every 10s)

  if (WiFi.status() == WL_CONNECTED) {
    if (!otaInProgress) {
      handleMQTT();         // MQTT loop and reconnection
    } // MQTT paused when OTA is active
  }

  #ifdef DEBUG_IR_PRINT
  debugIRInput();           // Optional IR test via Serial input
  #endif
}
