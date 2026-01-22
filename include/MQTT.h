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
// {"fan_speed":2,"temperature":24,"mode":"cool","louver":3,"power":true}
#define qos 1 // Quality of Service
#define cleanSession false

// Built MQTT topic buffers
// char mqtt_topic_sub_floor[64];
// char mqtt_topic_sub_room[64];
char mqtt_topic_sub_unit[64];
char mqtt_topic_pub_state[80];
char mqtt_topic_pub_identity[80];
char mqtt_topic_pub_deployment[80];
char mqtt_topic_pub_diagnostics[80];

// ─────────────────────────────────────────────
// 🔨 Topic Construction (call in setup)
// ─────────────────────────────────────────────
void setupMQTTTopics() {
  snprintf(mqtt_topic_sub_unit,        sizeof(mqtt_topic_sub_unit),        "%s/%s/%s/%s",           control_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_state,       sizeof(mqtt_topic_pub_state),       "%s/%s/%s/%s/state",       state_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_identity,    sizeof(mqtt_topic_pub_identity),    "%s/%s/%s/%s/identity",    state_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_deployment,  sizeof(mqtt_topic_pub_deployment),  "%s/%s/%s/%s/deployment",  state_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_diagnostics, sizeof(mqtt_topic_pub_diagnostics), "%s/%s/%s/%s/diagnostics", state_root, floor_id, room_id, unit_id);
}

// Clients and encoder instance
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
ACU_remote remote("MITSUBISHI_HEAVY_64");

// Heartbeat variables
char lastReceivedCommandJson[256] = {0};
unsigned long lastHeartbeatTime = 0;
char lastCommandTimestamp[30] = {0};
char lastChangeTimestamp[30] = {0};
const long HEARTBEAT_INTERVAL_MS = 15000; // 15 seconds
// ─────────────────────────────────────────────
// 📤 Publish state to dashboard
// ─────────────────────────────────────────────
void publishACUState(JsonDocument& sourceDoc) {
  if (!mqtt_client.connected()) {
    Serial.println("[MQTT] Not connected, skipping publish.");
    return;
  }

  JsonDocument stateDoc;
  
  // Map internal keys to schema
  if (sourceDoc.containsKey("temperature")) stateDoc["temperature"] = sourceDoc["temperature"];
  if (sourceDoc.containsKey("fan_speed"))    stateDoc["fan_speed"]    = sourceDoc["fan_speed"];
  if (sourceDoc.containsKey("mode"))        stateDoc["mode"]        = sourceDoc["mode"];
  if (sourceDoc.containsKey("louver"))      stateDoc["louver"]      = sourceDoc["louver"];
  if (sourceDoc.containsKey("power"))        stateDoc["power"]        = sourceDoc["power"];

  if (lastChangeTimestamp[0] != '\0') {
    stateDoc["last_change_ts"] = lastChangeTimestamp;
  }

  char output[256];
  size_t len = serializeJson(stateDoc, output, sizeof(output));

  bool ok = mqtt_client.publish(mqtt_topic_pub_state, (const uint8_t*)output, len, true); // retain = true
  if (ok) {
    Serial.println("[MQTT] Published state:");
    Serial.println(output);
  } else {
    Serial.println("[MQTT] Publish failed.");
  }
}

void publishIdentity() {
  if (!mqtt_client.connected()) return;
  JsonDocument doc;
  char clientIdStr[32];
  snprintf(clientIdStr, sizeof(clientIdStr), "ESP8266Client-%06X", ESP.getChipId());
  
  doc["device_id"] = clientIdStr;
  doc["acu_remote_model"] = ACU_REMOTE_MODEL; 
  doc["room_type"] = DEFINED_ROOM_TYPE;
  doc["department"] = DEFINED_DEPARTMENT;

  char output[256];
  serializeJson(doc, output);
  mqtt_client.publish(mqtt_topic_pub_identity, output, true); // Retain static info
}

void publishDeployment() {
  if (!mqtt_client.connected()) return;
  JsonDocument doc;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["deployment_date"] = DEFINED_DEPLOYMENT_DATE;
  doc["version_hash"] = DEFINED_VERSION_HASH;

  char output[256];
  serializeJson(doc, output);
  mqtt_client.publish(mqtt_topic_pub_deployment, output, true); // Retain static info
}

void publishDiagnostics() {
  if (!mqtt_client.connected()) return;
  JsonDocument doc;
  doc["uptime_s"] = millis() / 1000;
  doc["status"] = "online";
  
  // Add timestamp
  char timeBuffer[30];
  getTimestamp(timeBuffer, sizeof(timeBuffer));
  doc["last_diag_ts"] = timeBuffer;

  if (lastCommandTimestamp[0] != '\0') {
    doc["last_cmd_ts"] = lastCommandTimestamp;
  }

  char output[256];
  serializeJson(doc, output);
  mqtt_client.publish(mqtt_topic_pub_diagnostics, output, false); // Do not retain dynamic diagnostics
}

void publishOnReconnect() {
  publishIdentity();
  publishDeployment();
  publishDiagnostics();

  // Republish last known state if available
  if (lastReceivedCommandJson[0] != '\0') {
    JsonDocument doc;
    deserializeJson(doc, lastReceivedCommandJson);
    publishACUState(doc);
  }
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

  // Handle potential nested "state" object in new control format
  JsonObjectConst stateObj = doc.containsKey("state") ? doc["state"] : doc.as<JsonObjectConst>();

  if (remote.fromJSON(stateObj)) {
    uint64_t command = remote.encodeCommand();
    Serial.println("[MQTT] JSON parsed and encoded:");

    char binaryStr[100]; // Buffer for binary string representation (64 bits + 15 spaces + null)
    ACU_remote::toBinaryString(command, binaryStr, sizeof(binaryStr), true);
    Serial.println(binaryStr);

    yield(); // Allow ESP8266 to handle background tasks before blocking IR send

    size_t len = 0;
    parseBinaryToDurations(command, durations, len);
    irsend.sendRaw(durations, len, 38);

    // Create a mutable copy of the state for publishing/storage
    JsonDocument stateDoc;
    stateDoc.set(stateObj);

    // Check if state has changed compared to last received command
    bool stateChanged = false;
    if (lastReceivedCommandJson[0] == '\0') {
      stateChanged = true;
    } else {
      JsonDocument prevDoc;
      deserializeJson(prevDoc, lastReceivedCommandJson);
      if (stateDoc != prevDoc) stateChanged = true;
    }

    if (stateChanged) {
      char timeBuffer[30];
      getTimestamp(timeBuffer, sizeof(timeBuffer));
      strncpy(lastChangeTimestamp, timeBuffer, sizeof(lastChangeTimestamp));
    }

    publishACUState(stateDoc);

    // Update last command timestamp
    char timeBuffer[30];
    getTimestamp(timeBuffer, sizeof(timeBuffer));
    strncpy(lastCommandTimestamp, timeBuffer, sizeof(lastCommandTimestamp));
    publishDiagnostics(); // Update diagnostics (last_cmd_ts)
    serializeJson(stateDoc, lastReceivedCommandJson, sizeof(lastReceivedCommandJson));
  } else {
    Serial.println("[MQTT] Invalid command structure.");
  }
}

// ─────────────────────────────────────────────
// 📥 Handle Conflicting Topic (from retained/false-cleanSessions)
// ─────────────────────────────────────────────
bool topicMatchesModule(char* topic) {
  return strcmp(topic, mqtt_topic_sub_unit) == 0;
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

    if (mqtt_client.connect(clientId, mqtt_user, mqtt_pass, mqtt_topic_pub_diagnostics, qos, true, lwt_message, cleanSession)) {
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

// Publish heartbeat on connection
void publishHeartbeat() {
  if (mqtt_client.connected()) {
    if ((long)(millis() - lastHeartbeatTime) >= HEARTBEAT_INTERVAL_MS) {
      publishDiagnostics();
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
