/*
 * MQTT.h
 *
 * Handles MQTT setup and message processing for ESP8266.
 * Used to receive control commands and send ACU status updates.
 *
 * Features:
 * - Connects to MQTT broker (LAN or public)
 * - Subscribes to floor/room/unit topics
 * - Parses incoming JSON and sends IR commands
 * - Publishes current state back to dashboard
 *
 * Usage:
 * - Call setupMQTTTopics() before setupMQTT()
 * - Call setupMQTT() in setup()
 * - Call handleMQTT() in loop()
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "ACU_remote_encoder.h"
#include "ACU_IR_modulator.h"
#include <NTP.h>  // For getTimestamp()

// ─────────────────────────────────────────────
// 🔧 MQTT Broker Info
// ─────────────────────────────────────────────
// NOTE: LAN broker recommended in deployment
const char* mqtt_server = "airhub-soe.apc.edu.ph";  // For testing: broker.hivemq.com
const int mqtt_port = 1883;

const char* mqtt_user = "esp01m";     // your MQTT username
const char* mqtt_pass = "IR_Transceiver!"; // your MQTT password


// ─────────────────────────────────────────────
// 🧩 Topic Components (custom per device)
// ─────────────────────────────────────────────
// format: "floor_id/room_id/unit_id"
const char* floor_id = "8th Floor";  // Avoid using just 'floor' (reserved in math.h)
const char* room_id  = "807";
const char* unit_id  = "ACU1";

// sample json query:
// {"fanSpeed":2,"temperature":24,"mode":"cool","louver":3,"isOn":true}

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

// ─────────────────────────────────────────────
// 📤 Publish state to dashboard
// ─────────────────────────────────────────────
void publishDeviceState(const String& jsonPayload) {
  if (!mqtt_client.connected()) {
    Serial.println("[MQTT] Not connected, skipping publish.");
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, jsonPayload);
  if (err) {
    Serial.println("[MQTT] Failed to parse input JSON for publishing");
    return;
  }

  doc["timestamp"] = getTimestamp();

  String timestampedJson;
  serializeJson(doc, timestampedJson);

  bool ok = mqtt_client.publish(mqtt_topic_pub, timestampedJson.c_str(), true); // retain = true
  if (ok) {
    Serial.println("[MQTT] Published state with timestamp:");
    Serial.println(timestampedJson);
  } else {
    Serial.println("[MQTT] Publish failed.");
  }
}


// ─────────────────────────────────────────────
// 📥 Handle Incoming Commands
// ─────────────────────────────────────────────
void handleReceivedCommand(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("[MQTT] JSON parse failed");
    return;
  }

  String jsonStr;
  serializeJson(doc, jsonStr);

  if (remote.fromJSON(jsonStr)) {
    uint64_t command = remote.encodeCommand();
    Serial.println("[MQTT] JSON parsed and encoded:");
    Serial.println(ACU_remote::toBinaryString(command, true));

    size_t len = 0;
    parseBinaryToDurations(command, durations, len);
    irsend.sendRaw(durations, len, 38);

    publishDeviceState(jsonStr);
  } else {
    Serial.println("[MQTT] Invalid command structure.");
  }
}

// ─────────────────────────────────────────────
// 📡 Callback on Message Arrival
// ─────────────────────────────────────────────
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Incoming topic: ");
  Serial.println(topic);
  handleReceivedCommand(topic, payload, length);
}

// ─────────────────────────────────────────────
// ♻️ Reconnect (non-blocking)
// ─────────────────────────────────────────────
void mqtt_reconnect() {
  static unsigned long lastAttempt = 0;
  const unsigned long retryInterval = 10000;

  if (mqtt_client.connected()) return;

  unsigned long now = millis();
  if (now - lastAttempt >= retryInterval) {
    lastAttempt = now;

    Serial.print("[MQTT] Connecting... ");
    String clientId = "ESP8266Client-" + String(ESP.getChipId());

    optimistic_yield(10000);  // Yield for OTA

    if (mqtt_client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected.");
      mqtt_client.subscribe(mqtt_topic_sub_floor);
      mqtt_client.subscribe(mqtt_topic_sub_room);
      mqtt_client.subscribe(mqtt_topic_sub_unit);
    } else {
      Serial.print("failed (rc=");
      Serial.print(mqtt_client.state());
      Serial.println("), retrying...");
    }
  }
}

// ─────────────────────────────────────────────
// 🚀 Setup MQTT Client
// ─────────────────────────────────────────────
void setupMQTT() {
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(callback);
}

// ─────────────────────────────────────────────
// 🔄 Maintain MQTT Connection
// ─────────────────────────────────────────────
void handleMQTT() {
  if (!mqtt_client.connected()) {
    mqtt_reconnect();
  }
  mqtt_client.loop();
}
