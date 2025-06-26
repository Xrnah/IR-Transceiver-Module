/*
 * MQTT.h
 * 
 * Provides MQTT client setup and message handling for ESP8266 devices.
 * Integrates PubSubClient with Wi-Fi connectivity to receive and process
 * Air Conditioner control commands via MQTT.
 * 
 * Features:
 * - Connects to a specified MQTT broker.
 * - Subscribes to a control topic and handles incoming JSON payloads.
 * - Parses and validates JSON commands using ArduinoJson.
 * - Converts commands into encoded IR signals for the ACU remote.
 * - Includes reconnection logic and periodic client loop management.
 * 
 * Usage:
 * 1. Call setupMQTT() in setup() to configure MQTT client and callbacks.
 * 2. Call handleMQTT() inside loop() to maintain connection and process messages.
 * 3. Implement ACU_remote and IR sending functions for command encoding and sending.
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "ACU_remote_encoder.h"
#include "ACU_IR_modulator.h"

// MQTT broker settings
// ⚠️ Will soon implement an mDNS mqtt service running in domain level server.
const char* mqtt_server = "192.168.68.102";    // For Testing: broker.hivemq.com , test.mosquitto.org
const int mqtt_port = 1883;

// format: "floor/room/ACU#"
const char* mqtt_topic_sub = "8th Floor/809/ACU1";
const char* mqtt_topic_pub = "8th Floor/809/ACU1-status";
// sample json query:
// {"fanSpeed":2,"temperature":24,"mode":"cool","louver":3,"isOn":true}

WiFiClient espClient;            // Wi-Fi client for MQTT
PubSubClient mqtt_client(espClient);  // MQTT client instance

// ACU remote instance initialized with signature for encoding
ACU_remote remote("MITSUBISHI_HEAVY_64");

// ====== Publish current state back to MQTT dashboard ======
void publishDeviceState(const String& jsonPayload) {
  if (mqtt_client.connected()) {
    bool success = mqtt_client.publish(mqtt_topic_pub, jsonPayload.c_str(), true);  // retained = true
    if (success) {
      Serial.println("[MQTT] Published state to dashboard:");
      Serial.println(jsonPayload);
    } else {
      Serial.println("[MQTT] Failed to publish state.");
    }
  } else {
    Serial.println("[MQTT] Client not connected, publish skipped.");
  }
}

// ====== Handle incoming MQTT messages ======
void handleReceivedCommand(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;

  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("[MQTT] Failed to parse JSON");
    return;
  }

  // Convert JSON payload to string
  String jsonStr;
  serializeJson(doc, jsonStr);

  // Try to parse and encode command
  if (remote.fromJSON(jsonStr)) {
    uint64_t command = remote.encodeCommand();
    Serial.println("[MQTT] JSON parsed successfully");
    Serial.print("[MQTT] Encoded command: ");
    Serial.println(ACU_remote::toBinaryString(command, true));

    // Send IR signal
    size_t len = 0;
    parseBinaryToDurations(command, durations, len);
    irsend.sendRaw(durations, len, 38);

    // Relay the same JSON payload back to the MQTT dashboard
    publishDeviceState(jsonStr);

  } else {
    Serial.println("[MQTT] Invalid command structure");
  }
}

// ====== MQTT message callback ======
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Message arrived on topic: ");
  Serial.println(topic);
  handleReceivedCommand(topic, payload, length);
}

// ====== MQTT reconnect logic (non-blocking) ======
void mqtt_reconnect() {
  static unsigned long lastAttemptTime = 0;
  const unsigned long retryInterval = 10000;  // try every 10s

  if (mqtt_client.connected()) return;

  unsigned long now = millis();
  if (now - lastAttemptTime >= retryInterval) {
    lastAttemptTime = now;

    Serial.print("[MQTT] Attempting to connect...");
    String clientId = "ESP8266Client-" + String(ESP.getChipId());

    optimistic_yield(10000);  // Prevent OTA starvation

    if (mqtt_client.connect(clientId.c_str())) {
      Serial.println(" connected");
      mqtt_client.subscribe(mqtt_topic_sub);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" retrying in 10 seconds...");
    }
  }
}



// ====== Initialize MQTT client ======
void setupMQTT() {
  mqtt_client.setServer(mqtt_server, mqtt_port);  // Set broker and port
  mqtt_client.setCallback(callback);          // Set message callback
}

// ====== MQTT loop to be called in main loop() ======
void handleMQTT() {
  if (!mqtt_client.connected()) {
    mqtt_reconnect();  // Reconnect if disconnected
  }
  mqtt_client.loop();  // Process incoming messages
}
