#include <Arduino.h>

#include "WiFiManager.h"           // WiFi connection manager class
// #include "OTA_config.h"            // OTA setup and event handlers
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
WiFiManager wifiManager;                       // WiFi manager instance
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

  // Optional: Connect to a hidden network using credentials from secrets.h
  // #include "secrets.h" // Include this only if you uncomment the lines below
  // const char* ssid = HIDDEN_SSID; // HIDDEN_SSID is a #define from secrets.h
  // const char* pass = HIDDEN_PASS; // HIDDEN_PASS is a #define from secrets.h
  // wifiManager.connectToHidden(ssid, pass);

  wifiManager.autoConnectWithRetry();  // Smart WiFi retry
  // setupOTA();                  // Start OTA service
  setupMQTTTopics();          // Build MQTT topic strings
  setupMQTT();                // Start MQTT client
  setupTime();                // Configure NTP

  // Ensure MQTT is connected before publishing the initial status
  Serial.println("[MQTT] Waiting for initial connection...");
  while (!mqtt_client.connected()) {
    mqtt_reconnect();   
    mqtt_client.loop(); 
    delay(100);         // Small delay to yield processing time
  } powerOnPublish();

}

// ─────────────────────────────────────────────
// 🔁 Main Loop
// ─────────────────────────────────────────────
void loop() {
  ArduinoOTA.handle();

  wifiManager.checkConnection();              // WiFi health check (runs every 10s)

  if (WiFi.status() == WL_CONNECTED){
    if (!otaInProgress) {
      handleMQTT();  // auto reconnects + loops MQTT
    } // Pause MQTT Process when OTA is in Progress
  } // Only proceed when connected to WiFi
  
  #ifdef DEBUG_IR_PRINT
  debugIRInput();
  #endif

}

