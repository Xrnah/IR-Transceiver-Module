#include <Arduino.h>

// Initialized variables
#include "secrets.h"
#include "logging.h"

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
constexpr const char* k_log_tag = "MAIN";
constexpr unsigned long startup_delay_ms = 5000;
constexpr unsigned long wifi_loop_delay_ms = 10;

// DEBUG OPTIONS
// #define ENABLE_TIMER_ROUTINE
// #define ENABLE_IR_DEBUG_INPUT // Enables raw binary ACU instruction via Serial (requires LOG_SERIAL_ENABLE)

// ─────────────────────────────────────────────
// 🔧 Global Objects
// ─────────────────────────────────────────────
CustomWiFi::WiFiManager g_wifi_manager;           // WiFi manager instance
#if !USE_ACU_ADAPTER
  IRsend g_ir_send(ir_led_pin);                      // IR transmitter
  uint16_t g_durations[raw_data_length];             // Pulse duration buffer
  const IRProtocolConfig* g_selected_protocol = &k_mitsubishi_heavy_64;
#endif

#if ENABLE_TIMER_ROUTINE
  uint32_t g_last_timer_event_ms = 0;
  constexpr uint32_t timer_interval_ms = 1000; // 1 second
#endif
// ─────────────────────────────────────────────
// 🛠️ Setup (runs once on boot)
// ─────────────────────────────────────────────
void setup() {
  #if LOG_SERIAL_ENABLE
    Serial.begin(115200);
    delay(startup_delay_ms); // Startup delay for serial debugging. This is skipped in release builds.
    initLogging();
    logInfo(k_log_tag, "MCU Status: ON");
    String reset_reason = ESP.getResetReason();
    logInfo(k_log_tag, "Reset reason: %s", reset_reason.c_str());
  #endif
  
#if !USE_ACU_ADAPTER
  g_ir_send.begin();
#endif

  g_wifi_manager.begin(HIDDEN_SSID, HIDDEN_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    g_wifi_manager.handleConnection(); // Let the state machine run
    delay(wifi_loop_delay_ms); // Small delay to prevent busy-waiting
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
  g_wifi_manager.handleConnection(); 

  updateConnectionStats();

  if (WiFi.status() == WL_CONNECTED) {
    handleMQTT();
  }

  #if ENABLE_TIMER_ROUTINE
  uint32_t now_ms = millis();
    if ((uint32_t)(now_ms - g_last_timer_event_ms) >= timer_interval_ms) {
    g_last_timer_event_ms = now_ms; // Update the time of the last event

    logDebug(k_log_tag, "Periodic task executed.");
    logDebug(k_log_tag, "Free heap: %u", ESP.getFreeHeap());
    
    }    
  #endif

  #if !USE_ACU_ADAPTER
    #if LOG_SERIAL_ENABLE
      #ifdef ENABLE_IR_DEBUG_INPUT
      debugIRInput();           // Optional IR test via Serial input
      #endif
    #endif
  #endif
}
