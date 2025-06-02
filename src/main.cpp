#include <Arduino.h>

#include "OTA_config.h"           // OTA configuration and setup functions
#include "ACU_remote_encoder.h"   // ACU remote command encoding
#include "ACU_IR_modulator.h"     // IR modulation and signal generation

// MQTT client wrapper (uses ACU_remote::toJSON() for messaging)
#include "MQTT.h"                 // Uses the PubSubClient library

// Debug flag for serial IR input/output (uncomment to enable)
// #define DEBUG_IR_PRINT

// Constants
#define ACUsignature "MITSUBISHI_HEAVY_64"   // Signature used by ACU encoder
#define kIrLedPin     4                      // GPIO pin connected to IR LED
#define rawDataLength 133                    // Length of IR raw duration buffer

// Global objects
IRsend irsend(kIrLedPin);                   // IR sender instance on specified pin
uint16_t durations[rawDataLength];          // Buffer for IR signal durations

ACU_remote APC_ACU(ACUsignature);           // ACU remote encoder instance
const IRProtocolConfig* selectedProtocol = &MITSUBISHI_HEAVY_64;  // Active IR protocol config

void setup() {
  Serial.begin(115200);          // Initialize serial for debugging
  irsend.begin();                // Initialize IR sender hardware

  Serial.println("\nMCU Status: ON"); // Indicate MCU startup

  setupOTA();                   // Initialize OTA update functionality
  setupMQTT();                  // Initialize MQTT communication
}

void loop() {
  ArduinoOTA.handle();  // Must be first to ensure responsiveness

  if (!otaInProgress) {
    mqtt_reconnect();
    mqtt_client.loop();  // MQTT client processing
  }
  
  #ifdef DEBUG_IR_PRINT
  debugIRInput();  // Optional debug input from serial
  #endif
}
