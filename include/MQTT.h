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
const char* floor_id = DEFINED_FLOOR;
const char* room_id = DEFINED_ROOM;
const char* unit_id = DEFINED_UNIT;

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

  if (!doc.containsKey("timestamp")) {
    doc["timestamp"] = getTimestamp();
  }

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

void powerOnPublish() {
  StaticJsonDocument<256> doc;
  String powerOnTimestamp = "Powered ON @: " + getTimestamp();
  String clientId = "ESP8266Client-" + String(ESP.getChipId());

  doc["isOn"] = "The device recently powered ON, send a command to update";
  doc["timestamp"] = powerOnTimestamp;
  doc["deviceID"] = clientId;
  doc["deviceIP"] = WiFi.localIP().toString();

  String initial_status;
  serializeJson(doc, initial_status);
  publishDeviceState(initial_status); // Publishes the JSON string with the custom timestamp
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
// 📥 Handle Conflicting Topic (from retained/false-cleanSessions)
// ─────────────────────────────────────────────
bool topicMatchesModule(char* topic) {
  // Expected forms:
  // control/floor
  // control/floor/room
  // control/floor/room/unit

  String t = String(topic);
  t.trim();
  if (!t.startsWith("control/")) return false;

  // Split by '/'
  int idx1 = t.indexOf('/', 0);           // after 'control'
  int idx2 = t.indexOf('/', idx1 + 1);    // after floor
  int idx3 = t.indexOf('/', idx2 + 1);    // after room

  String floor = t.substring(idx1 + 1, (idx2 == -1 ? t.length() : idx2));
  if (floor != floor_id) return false;

  // Only floor match → OK
  if (idx2 == -1) return true;

  String room = t.substring(idx2 + 1, (idx3 == -1 ? t.length() : idx3));
  if (room != room_id) return false;

  // Floor + room match → OK
  if (idx3 == -1) return true;

  String unit = t.substring(idx3 + 1);
  return (unit == unit_id);
}

// ─────────────────────────────────────────────
// 📡 Callback on Message Arrival
// ─────────────────────────────────────────────
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Incoming topic: ");
  Serial.println(topic);

  if (!topicMatchesModule(topic)) {
    Serial.println("[MQTT] Topic rejected by filter.");
    return;
  }

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

    if (mqtt_client.connect(clientId.c_str(), mqtt_user, mqtt_pass, mqtt_topic_pub, 1, true, lwt_message, cleanSession)) {
      Serial.println("connected.");
      mqtt_client.subscribe(mqtt_topic_sub_floor, qos);
      mqtt_client.subscribe(mqtt_topic_sub_room, qos);
      mqtt_client.subscribe(mqtt_topic_sub_unit, qos);
    } else {
      Serial.print("[MQTT] failed (rc=");
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
  mqtt_client.setKeepAlive(10); // 10 seconds
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
