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
#include <pgmspace.h>
#include "secrets.h" // Credentials

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
const char* state_root = STATE_PATH;
const char* control_root = CONTROL_PATH;
const char* floor_id = DEFINED_FLOOR;
const char* room_id = DEFINED_ROOM;
const char* unit_id = DEFINED_UNIT;

const char lwt_message_json[] PROGMEM = "{\"status\":\"offline\"}";
char lwt_message[sizeof(lwt_message_json)]; // Buffer to hold LWT message from PROGMEM

// sample json query:
// {"fanSpeed":2,"temperature":24,"mode":"cool","louver":3,"isOn":true}
#define qos 1 // Quality of Service
#define cleanSession false

// Built MQTT topic buffers
// char mqtt_topic_sub_floor[64];
// char mqtt_topic_sub_room[64];
char mqtt_topic_sub_unit[64];
char mqtt_topic_pub[80];

// ─────────────────────────────────────────────
// 🔨 Topic Construction (call in setup)
// ─────────────────────────────────────────────
void setupMQTTTopics() {
  snprintf(mqtt_topic_sub_unit,  sizeof(mqtt_topic_sub_unit),  "%s/%s/%s/%s", control_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub,       sizeof(mqtt_topic_pub),       "%s/%s/%s/%s-status", state_root, floor_id, room_id, unit_id);
}

// Clients and encoder instance
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
ACU_remote remote("MITSUBISHI_HEAVY_64");

// Heartbeat variables
char lastReceivedCommandJson[256] = {0};
unsigned long lastHeartbeatTime = 0;
const long HEARTBEAT_INTERVAL_MS = 15000; // 15 seconds
// ─────────────────────────────────────────────
// 📤 Publish state to dashboard
// ─────────────────────────────────────────────
void publishDeviceState(JsonDocument& doc) {
  if (!mqtt_client.connected()) {
    Serial.println("[MQTT] Not connected, skipping publish.");
    return;
  }

  // Add/update timestamp just before publishing
  char timeBuffer[30];
  getTimestamp(timeBuffer, sizeof(timeBuffer));
  doc["timestamp"] = timeBuffer;

  char output[512];
  size_t len = serializeJson(doc, output, sizeof(output));

  bool ok = mqtt_client.publish(mqtt_topic_pub, (const uint8_t*)output, len, true); // retain = true
  if (ok) {
    Serial.println("[MQTT] Published state with timestamp:");
    Serial.println(output);
  } else {
    Serial.println("[MQTT] Publish failed.");
  }
}

void publishOnReconnect() {
  JsonDocument doc;
  char clientIdStr[32];
  snprintf(clientIdStr, sizeof(clientIdStr), "ESP8266Client-%06X", ESP.getChipId());

  if (lastReceivedCommandJson[0] != '\0') {
    deserializeJson(doc, lastReceivedCommandJson);
    doc["status"] = "Connection Restored";
  } else {
    doc["status"] = "online";
  }
  // Timestamp will be added by publishDeviceState
  doc["deviceID"] = clientIdStr;
  char ipStr[16];
  WiFi.localIP().toString().toCharArray(ipStr, sizeof(ipStr));
  doc["deviceIP"] = ipStr;

  publishDeviceState(doc);
}

// ─────────────────────────────────────────────
// 📥 Handle Incoming Commands
// ─────────────────────────────────────────────
void handleReceivedCommand(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("[MQTT] JSON parse failed");
    return;
  }

  if (remote.fromJSON(doc.as<JsonObjectConst>())) {
    uint64_t command = remote.encodeCommand();
    Serial.println("[MQTT] JSON parsed and encoded:");

    char binaryStr[100]; // Buffer for binary string representation (64 bits + 15 spaces + null)
    ACU_remote::toBinaryString(command, binaryStr, sizeof(binaryStr), true);
    Serial.println(binaryStr);

    yield(); // Allow ESP8266 to handle background tasks before blocking IR send

    size_t len = 0;
    parseBinaryToDurations(command, durations, len);
    irsend.sendRaw(durations, len, 38);

    // Publish the state (for instanteneous feedback)
    // then for heartbeat
    publishDeviceState(doc);
    serializeJson(doc, lastReceivedCommandJson, sizeof(lastReceivedCommandJson));
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
  const char* prefix = "control/";
  if (strncmp(topic, prefix, strlen(prefix)) != 0) {
      return false;
  }

  char topicCopy[128];
  strncpy(topicCopy, topic, sizeof(topicCopy) - 1);
  topicCopy[sizeof(topicCopy) - 1] = '\0';

  char* rest = topicCopy + strlen(prefix);
  char* part;

  // Check floor
  part = strtok(rest, "/");
  if (!part || strcmp(part, floor_id) != 0) return false;

  // Check room
  part = strtok(NULL, "/");
  if (!part) return true; // Matches floor only
  if (strcmp(part, room_id) != 0) return false;

  // Check unit
  part = strtok(NULL, "/");
  if (!part) return true; // Matches floor/room
  return (strcmp(part, unit_id) == 0);
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
    
    static char clientId[32];
    static bool idInit = false;

    if (!idInit) {
      snprintf(clientId, sizeof(clientId),
              "ESP8266Client-%06X", ESP.getChipId());
      // Copy LWT message from PROGMEM to RAM buffer
      strcpy_P(lwt_message, lwt_message_json);
      idInit = true;
    }

    if (mqtt_client.connect(clientId, mqtt_user, mqtt_pass, mqtt_topic_pub, qos, true, lwt_message, cleanSession)) {
      Serial.println("connected.");
      // mqtt_client.subscribe(mqtt_topic_sub_floor, qos);
      // mqtt_client.subscribe(mqtt_topic_sub_room, qos);
      mqtt_client.subscribe(mqtt_topic_sub_unit, qos);
      publishOnReconnect();
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

void publishHeartbeat() {
// Heartbeat: Publish the last received command periodically
  if (mqtt_client.connected() && lastReceivedCommandJson[0] != '\0') {
    if ((long)(millis() - lastHeartbeatTime) >= HEARTBEAT_INTERVAL_MS) {
      JsonDocument doc;
      deserializeJson(doc, lastReceivedCommandJson);

      // This will re-publish the last state with an updated timestamp
      publishDeviceState(doc);
      lastHeartbeatTime = millis();
    }
  }
}

// ─────────────────────────────────────────────
// 🔄 Maintain MQTT Connection
// ─────────────────────────────────────────────
void handleMQTT() {
  if (!mqtt_client.connected()) {
    mqtt_reconnect();
    yield();
  } 

  mqtt_client.loop();
  yield();

  publishHeartbeat();
  yield();

}

void mqttDisconnect() {
  if (mqtt_client.connected()) {
    mqtt_client.disconnect();
  }
}
