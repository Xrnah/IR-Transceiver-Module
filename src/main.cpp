
#include <Arduino.h>

#include "WiFiManager.h"
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

  delay(5000);                  // For Serial Print Consistency; Comment on production

  Serial.println("\nMCU Status: ON"); // Indicate MCU startup

  const char* hardcoded_ssid = "Mansion 103";
  const char* hardcoded_pass = "Connection_503";
  autoConnectWiFi(hardcoded_ssid, hardcoded_pass);

  // autoConnectWiFiWithRetry();   // Wifi
  setupOTA();                   // Initialize OTA update functionality
  setupMQTT();                  // Initialize MQTT communication
}

void loop() {
  ArduinoOTA.handle();

  checkWiFi();  // every 10s

  if (WiFi.status() == WL_CONNECTED){
    if (!otaInProgress) {
      handleMQTT();  // auto reconnects + loops MQTT
    } // Pause MQTT Process when OTA is in Progress
  } // Only proceed when connected to WiFi
  
  #ifdef DEBUG_IR_PRINT
  debugIRInput();
  #endif

}
