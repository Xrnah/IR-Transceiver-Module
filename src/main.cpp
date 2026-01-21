#include <Arduino.h>

// Initialized variables
#include "secrets.h"

// Custom Libraries
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
CustomWiFi::WiFiManager wifiManager;           // WiFi manager instance
IRsend irsend(kIrLedPin);                      // IR transmitter
uint16_t durations[rawDataLength];             // Pulse duration buffer
const IRProtocolConfig* selectedProtocol = &MITSUBISHI_HEAVY_64;

// ─────────────────────────────────────────────
// 🛠️ Setup (runs once on boot)
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  irsend.begin();             // IR pin setup
  delay(5000);                // Serial startup delay (skip in production)
  Serial.println("\n🔌 MCU Status: ON");

  // First, try to connect to a known hidden network. This is a blocking call.
  // If this fails, the non-blocking manager will take over.
  wifiManager.connectToHidden(HIDDEN_SSID, HIDDEN_PASS);

  wifiManager.begin(); // Start the non-blocking WiFi connection process

  // Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    wifiManager.handleConnection(); // Let the state machine run
    delay(10); // Small delay to prevent busy-waiting
  }

  // setupOTA();                  // Start OTA service
  setupMQTTTopics();          // Build MQTT topic strings
  setupMQTT();                // Start MQTT client
  setupTime();                // Configure NTP
}

// ─────────────────────────────────────────────
// 🔁 Main Loop
// ─────────────────────────────────────────────
void loop() {
  wifiManager.handleConnection(); 
  
  if (WiFi.status() == WL_CONNECTED) {
    handleMQTT();
  }

  #ifdef DEBUG_IR_PRINT
  debugIRInput();           // Optional IR test via Serial input
  #endif
}
