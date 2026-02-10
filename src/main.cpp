#include <Arduino.h>

// Initialized variables
#include "secrets.h"

// Fallback definition
#ifndef HIDDEN_SSID
#define HIDDEN_SSID ""
#endif

#ifndef HIDDEN_PASS
#define HIDDEN_PASS ""
#endif

// IR send default pipeline selection
// 1 = use protocol adapters (IRremoteESP8266)
// 0 = use legacy raw IR modulator (64-bit custom)
#ifndef USE_ACU_ADAPTER
  #define USE_ACU_ADAPTER 0
#endif

// Custom Libraries
#include "WiFiManager.h"           // WiFi connection manager class
// #include "OTA_config.h"            // OTA setup and event handlers
#include "ACU_remote_encoder.h"    // IR command generator (ACU signature)
#if !USE_ACU_ADAPTER
  #include "ACU_IR_modulator.h"      // Converts command to IR waveform
#endif
#include "MQTT.h"                  // MQTT messaging (PubSubClient wrapper)

// ─────────────────────────────────────────────
// 📡 Configuration
// ─────────────────────────────────────────────
#if !USE_ACU_ADAPTER
  #define kIrLedPin      4                       // IR LED GPIO pin
  #define rawDataLength  133                     // Raw buffer size for IR pulse timing
#endif

// DEBUG OPTIONS
// #define DEBUG_MODE // Enables serial
// #define DEBUG_IR_PRINT // Enables raw binary ACU instruction in serial
// #define DEBUG_MODE_TIMER // Enables timed functions in loop

// ─────────────────────────────────────────────
// 🔧 Global Objects
// ─────────────────────────────────────────────
CustomWiFi::WiFiManager wifiManager;           // WiFi manager instance
#if !USE_ACU_ADAPTER
  IRsend irsend(kIrLedPin);                      // IR transmitter
  uint16_t durations[rawDataLength];             // Pulse duration buffer
  const IRProtocolConfig* selectedProtocol = &MITSUBISHI_HEAVY_64;
#endif

#ifdef DEBUG_MODE_TIMER
  volatile uint32_t lastTimerEvent = 0;
  const uint32_t timerInterval = 1000; // 1 second
#endif
// ─────────────────────────────────────────────
// 🛠️ Setup (runs once on boot)
// ─────────────────────────────────────────────
void setup() {
  #ifdef DEBUG_MODE
    Serial.begin(115200);
    delay(5000); // Startup delay for serial debugging. This is skipped in release builds.
    Serial.println("\n🔌 MCU Status: ON");
    Serial.print("Reset reason: ");
    Serial.println(ESP.getResetReason());
  #endif
  
#if !USE_ACU_ADAPTER
  irsend.begin();
#endif

  wifiManager.begin(HIDDEN_SSID, HIDDEN_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    wifiManager.handleConnection(); // Let the state machine run
    delay(10); // Small delay to prevent busy-waiting
  }

  // setupOTA();                  // Start OTA service
  setupMqttTopics();          // Build MQTT topic strings
  setupMqtt();                // Start MQTT client
  setupTime();                // Configure NTP
}

// ─────────────────────────────────────────────
// 🔁 Main Loop
// ─────────────────────────────────────────────
void loop() {
  wifiManager.handleConnection(); 

  updateConnectionStats();

  if (WiFi.status() == WL_CONNECTED) {
    handleMqtt();
  }

  #ifdef DEBUG_MODE_TIMER
  uint32_t now = millis();
    if ((uint32_t)(now - lastTimerEvent) >= timerInterval) {
    lastTimerEvent = millis(); // Update the time of the last event

    Serial.println("[TIMER] Periodic task executed.");
    
    // Task: Free heap monitor
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());
    
    }    
  #endif

  #if !USE_ACU_ADAPTER
    #ifdef DEBUG_IR_PRINT
    debugIRInput();           // Optional IR test via Serial input
    #endif
  #endif
}
