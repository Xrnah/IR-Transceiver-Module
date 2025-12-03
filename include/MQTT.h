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
#include <NTP.h>  // For getTimestamp()
#include "secrets.h" // Secret credentials

// ─────────────────────────────────────────────
// 🔧 MQTT Broker Info
// ─────────────────────────────────────────────
// Broker address, user, and password are defined in 'secrets.h'
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;

const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PASS;

// ─────────────────────────────────────────────
// 🧩 Topic Components (custom per device)
// ─────────────────────────────────────────────
// format: "floor_id/room_id/unit_id"
extern const char* floor_id;  // Avoid using just 'floor' (reserved in math.h)
extern const char* room_id;
extern const char* unit_id;

char lwt_message[] = "{\"status\":\"offline\"}";

// sample json query:
// {"fanSpeed":2,"temperature":24,"mode":"cool","louver":3,"isOn":true}
#define qos 1
#define cleanSession false

// Built MQTT topic buffers
char mqtt_topic_sub_floor[64];
char mqtt_topic_sub_room[64];
char mqtt_topic_sub_unit[64];
char mqtt_topic_pub[80];

// ─────────────────────────────────────────────
// 🔨 Topic Construction (call in setup)
// ─────────────────────────────────────────────
void setupMQTTTopics() {
  snprintf(mqtt_topic_sub_floor, sizeof(mqtt_topic_sub_floor), "control/%s", floor_id);
  snprintf(mqtt_topic_sub_room,  sizeof(mqtt_topic_sub_room),  "control/%s/%s", floor_id, room_id);
  snprintf(mqtt_topic_sub_unit,  sizeof(mqtt_topic_sub_unit),  "control/%s/%s/%s", floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub,       sizeof(mqtt_topic_pub),       "esp/%s/%s/%s-status", floor_id, room_id, unit_id);
}

// Clients and encoder instance
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
ACU_remote remote("MITSUBISHI_HEAVY_64");

// ====== Handle incoming MQTT messages ======
void handleReceivedCommand(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;  // JSON document buffer

  // Deserialize JSON payload
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("[MQTT] Failed to parse JSON");
    return;  // Exit if JSON invalid
  }

  // Convert JSON payload to string for remote parsing
  String jsonStr;
  serializeJson(doc, jsonStr);

  // Parse JSON and encode command
  if (remote.fromJSON(jsonStr)) {
    uint64_t command = remote.encodeCommand();
    Serial.println("[MQTT] JSON parsed successfully");
    Serial.print("[MQTT] Encoded command: ");
    Serial.println(ACU_remote::toBinaryString(command, true));

    // Convert encoded command into durations and send IR signal
    size_t len = 0;
    parseBinaryToDurations(command, durations, len);
    irsend.sendRaw(durations, len, 38);  // Send at 38kHz carrier frequency
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
