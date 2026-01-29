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

// =================================================================================
// 1. CONFIGURATION & CONSTANTS
// =================================================================================

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

// ─────────────────────────────────────────────
// ⚙️ MQTT Settings
// ─────────────────────────────────────────────
  const char lwt_message_json[] PROGMEM = "{\"status\":\"offline\"}";
  #define qos 1 // Quality of Service
  #define cleanSession false
  const long HEARTBEAT_INTERVAL_MS = 15000; // 15 seconds

// =================================================================================
// 2. GLOBAL OBJECTS & STATE
// =================================================================================

// ─────────────────────────────────────────────
// 📦 Clients & Instances
// ─────────────────────────────────────────────
  WiFiClient espClient;
  PubSubClient mqtt_client(espClient);
  ACU_remote remote("MITSUBISHI_HEAVY_64");
// ─────────────────────────────────────────────
// 📝 Buffers
// ─────────────────────────────────────────────
  // MQTT topic buffers
  char mqtt_topic_sub_unit[64];
  char mqtt_topic_pub_state[80];
  char mqtt_topic_pub_identity[80];
  char mqtt_topic_pub_deployment[80];
  char mqtt_topic_pub_diagnostics[80];
  char mqtt_topic_pub_metrics[80];

  // MQTT Queue for ISR-safe decoupling
  struct MQTTQueueItem {
      char topic[64];
      char payload[256];
      unsigned int length;
  };
  const uint8_t MQTT_QUEUE_SIZE = 5;
  MQTTQueueItem mqttQueue[MQTT_QUEUE_SIZE];
  volatile uint8_t mqttQueueHead = 0;
  volatile uint8_t mqttQueueTail = 0;

  // Heartbeat & Timestamp buffers
  char lastReceivedCommandJson[256] = {0};
  char lastCommandTimestamp[30] = {0};
  char lastChangeTimestamp[30] = {0};
  unsigned long lastHeartbeatTime = 0;

  bool mqttPublishInProgress = false; // lock to prevent overlapping publishes

  uint32_t lastStateCRC = 0;
  bool stateInitialized = false;
  char lwt_message[sizeof(lwt_message_json)];

  static ACUState lastState = {0};

  static StaticJsonDocument<192> diagDoc;
  static StaticJsonDocument<384> metricsDoc;

  // Pre-allocated serialization buffers
  static char diagOutput[128];
  static char metricsOutput[384];

// ─────────────────────────────────────────────
// 📊 Metrics & Counters
// ─────────────────────────────────────────────
  // Connection Stats
  unsigned long wifi_connect_ts = 0;
  unsigned long mqtt_connect_ts = 0;
  unsigned int wifi_disconnect_counter = 0;
  unsigned int mqtt_disconnect_counter = 0;
  unsigned int commands_received_counter = 0;
  unsigned int commands_executed_counter = 0;
  bool prev_wifi_status = false;
  bool prev_mqtt_status = false;

  // Cumulative availability counters
  uint32_t wifi_connected_total_s = 0;
  uint32_t mqtt_connected_total_s = 0;
  unsigned long last_wifi_update_ms = 0;
  unsigned long last_mqtt_update_ms = 0;

  // Command latency metrics
  uint32_t last_cmd_latency_ms = 0;
  uint32_t avg_cmd_latency_ms = 0;

  // Command failure counters
  uint32_t commands_failed_parse = 0;
  uint32_t commands_failed_ir = 0;

  // MQTT publish failures
  uint32_t mqtt_publish_failures = 0;

// =================================================================================
// 3. INTERNAL UTILITIES
// =================================================================================

// ─────────────────────────────────────────────
// 🔨 Topic Initialization
// ─────────────────────────────────────────────
void setupMQTTTopics() {
  snprintf(mqtt_topic_sub_unit,        sizeof(mqtt_topic_sub_unit),        "%s/%s/%s/%s",           control_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_state,       sizeof(mqtt_topic_pub_state),       "%s/%s/%s/%s/state",       state_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_identity,    sizeof(mqtt_topic_pub_identity),    "%s/%s/%s/%s/identity",    state_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_deployment,  sizeof(mqtt_topic_pub_deployment),  "%s/%s/%s/%s/deployment",  state_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_diagnostics, sizeof(mqtt_topic_pub_diagnostics), "%s/%s/%s/%s/diagnostics", state_root, floor_id, room_id, unit_id);
  snprintf(mqtt_topic_pub_metrics,     sizeof(mqtt_topic_pub_metrics),     "%s/%s/%s/%s/metrics",     state_root, floor_id, room_id, unit_id);
}

// ─────────────────────────────────────────────
// 📈 Stats Updater
// ─────────────────────────────────────────────
void updateConnectionStats() {
  unsigned long now = millis();

  // WiFi Stats
  bool current_wifi = (WiFi.status() == WL_CONNECTED);
  if (current_wifi) {
    if (last_wifi_update_ms == 0) {
      last_wifi_update_ms = now;
    } else if (now - last_wifi_update_ms >= 1000) {
      wifi_connected_total_s += (now - last_wifi_update_ms) / 1000;
      last_wifi_update_ms = now;
    }
  } else {
    last_wifi_update_ms = 0;
  }

  if (current_wifi && !prev_wifi_status) {
    wifi_connect_ts = now;
  } 
  else if (!current_wifi && prev_wifi_status) {
    wifi_disconnect_counter++;
  }
  prev_wifi_status = current_wifi;

  // MQTT Stats
  bool current_mqtt = mqtt_client.connected();
  if (current_mqtt) {
    if (last_mqtt_update_ms == 0) {
      last_mqtt_update_ms = now;
    } else if (now - last_mqtt_update_ms >= 1000) {
      mqtt_connected_total_s += (now - last_mqtt_update_ms) / 1000;
      last_mqtt_update_ms = now;
    }
  } else {
    last_mqtt_update_ms = 0;
  }

  if (current_mqtt && !prev_mqtt_status) {
    mqtt_connect_ts = now;
  } 
  else if (!current_mqtt && prev_mqtt_status) {
    mqtt_disconnect_counter++;
  }
  prev_mqtt_status = current_mqtt;
}

bool topicMatchesModule(char* topic) {
  return strcmp(topic, mqtt_topic_sub_unit) == 0;
}

// =================================================================================
// 4. PUBLISHING FUNCTIONS
// =================================================================================
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

  char output[192];
  size_t len = serializeJson(stateDoc, output, sizeof(output));

  bool ok = mqtt_client.publish(mqtt_topic_pub_state, (const uint8_t*)output, len, true); // retain = true
  if (ok) {
    Serial.println("[MQTT] Published state:");
    Serial.println(output);
  } else {
    Serial.println("[MQTT] Publish failed.");
    mqtt_publish_failures++;
  }
  stateDoc.clear();
}

void publishIdentity() {
  if (!mqtt_client.connected()) return;
  StaticJsonDocument<512> doc;
  char clientIdStr[32];
  snprintf(clientIdStr, sizeof(clientIdStr), "ESP8266Client-%06X", ESP.getChipId());
  
  doc["device_id"] = clientIdStr;
  doc["mac_address"] = WiFi.macAddress();
  doc["acu_remote_model"] = ACU_REMOTE_MODEL; 
  // doc["room_type"] = DEFINED_ROOM_TYPE;
  doc["room_type_id"] = DEFINED_ROOM_TYPE_ID;
  doc["department"] = DEFINED_DEPARTMENT;

  char output[384];
  if (doc.overflowed()) Serial.println("⚠ Identity JSON doc overflow");
  size_t n = serializeJson(doc, output, sizeof(output));
  if (n >= sizeof(output)) Serial.println("⚠ Identity output truncated");

  mqtt_client.publish(mqtt_topic_pub_identity, output, true); // Retain identity info
  doc.clear();
}

void publishDeployment() {
  if (!mqtt_client.connected()) return;
  StaticJsonDocument<256> doc;
  doc["ip_address"] = WiFi.localIP().toString();
  // doc["deployment_date"] = DEFINED_DEPLOYMENT_DATE;
  // doc["version_hash"] = DEFINED_VERSION_HASH;
  doc["version_hash"] = GIT_HASH;
  doc["build_timestamp"] = BUILD_TIMESTAMP;

  char output[192];
  if (doc.overflowed()) Serial.println("⚠ Deployment JSON doc overflow");
  size_t n = serializeJson(doc, output, sizeof(output));
  if (n >= sizeof(output)) Serial.println("⚠ Deployment output truncated");
  mqtt_client.publish(mqtt_topic_pub_deployment, output, true); // Retain deployment info

  doc.clear();
}

void publishDiagnostics() {
    if (!mqtt_client.connected()) return;

    // Check lock
    if (mqttPublishInProgress) return;
    mqttPublishInProgress = true;

    diagDoc["status"] = "online";

    char timeBuffer[30];
    getTimestamp(timeBuffer, sizeof(timeBuffer));
    diagDoc["last_diag_ts"] = timeBuffer;

    if (lastCommandTimestamp[0] != '\0') {
        diagDoc["last_cmd_ts"] = lastCommandTimestamp;
    }
    
    diagDoc["cpu_mhz"] = ESP.getCpuFreqMHz();
    diagDoc["reset_reason"] = ESP.getResetReason();


    // Serialize into pre-allocated global buffer
    size_t n = serializeJson(diagDoc, diagOutput, sizeof(diagOutput));

    bool ok = mqtt_client.publish(
        mqtt_topic_pub_diagnostics,
        (const uint8_t*)diagOutput,
        n,
        false // no retain
    );

    if (!ok) mqtt_publish_failures++;

    mqttPublishInProgress = false;
    diagDoc.clear();
}

void publishMetrics() {
    if (!mqtt_client.connected()) return;

    if (mqttPublishInProgress) return;
    mqttPublishInProgress = true;

    unsigned long now = millis();

    metricsDoc["uptime_s"] = now / 1000;
    metricsDoc["wifi_uptime_s"] = (WiFi.status() == WL_CONNECTED) ? (now - wifi_connect_ts) / 1000 : 0;
    metricsDoc["mqtt_uptime_s"] = mqtt_client.connected() ? (now - mqtt_connect_ts) / 1000 : 0;

    metricsDoc["wifi_conn_total_s"] = wifi_connected_total_s;
    metricsDoc["mqtt_conn_total_s"] = mqtt_connected_total_s;

    metricsDoc["wifi_disc"] = wifi_disconnect_counter;
    metricsDoc["mqtt_disc"] = mqtt_disconnect_counter;
    metricsDoc["cmd_rx"] = commands_received_counter;
    metricsDoc["cmd_exec"] = commands_executed_counter;
    metricsDoc["cmd_fail_parse"] = commands_failed_parse;
    metricsDoc["cmd_latency_ms"] = last_cmd_latency_ms;
    metricsDoc["cmd_latency_avg_ms"] = avg_cmd_latency_ms;

    metricsDoc["free_heap"] = ESP.getFreeHeap();
    metricsDoc["heap_frag"] = ESP.getHeapFragmentation();
    metricsDoc["wifi_rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
    metricsDoc["mqtt_pub_fail"] = mqtt_publish_failures;

    // Serialize into pre-allocated global buffer
    size_t n = serializeJson(metricsDoc, metricsOutput, sizeof(metricsOutput));

    bool ok = mqtt_client.publish(
        mqtt_topic_pub_metrics,
        (const uint8_t*)metricsOutput,
        n,
        false
    );

    if (!ok) mqtt_publish_failures++;

    mqttPublishInProgress = false;
    metricsDoc.clear();

    // Optional debug
    // Serial.println("[MQTT] Metrics published:");
    // Serial.println(metricsOutput);
}


void publishOnReconnect() {
  publishIdentity();
  publishDeployment();
  publishDiagnostics();
  publishMetrics();

  // Republish last known state if available
  if (lastReceivedCommandJson[0] != '\0') {
    StaticJsonDocument<384> doc;
    deserializeJson(doc, lastReceivedCommandJson);
    publishACUState(doc);
  }
}

// Publish heartbeat on connection
void publishHeartbeat() {
    if (!mqtt_client.connected()) return;

    if ((long)(millis() - lastHeartbeatTime) >= HEARTBEAT_INTERVAL_MS) {
        // Only one function sets the lock, so heartbeat + command never overlap
        publishDiagnostics();
        publishMetrics();
        lastHeartbeatTime = millis();
    }
}


// =================================================================================
// 5. COMMAND HANDLING
// =================================================================================

void handleReceivedCommand(char* topic, byte* payload, unsigned int length) {
  unsigned long t_rx = millis();

  // Deserialize incoming JSON
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("[MQTT] JSON parse failed");
    commands_failed_parse++;
    return;
  }

  commands_received_counter++;

  // Handle potential nested "state" object
  JsonObjectConst stateObj = doc.containsKey("state") ? doc["state"] : doc.as<JsonObjectConst>();

  if (!remote.fromJSON(stateObj)) {
    Serial.println("[MQTT] Invalid command structure.");
    return;
  }

  // Encode IR command
  uint64_t command = remote.encodeCommand();
  char binaryStr[100];
  ACU_remote::toBinaryString(command, binaryStr, sizeof(binaryStr), true);
  Serial.println("[MQTT] JSON parsed and encoded:");
  Serial.println(binaryStr);

  yield(); // Allow ESP8266 background tasks

  size_t len = 0;
  // Send IR if parsing is successful
  if (parseBinaryToDurations(command, durations, len)) {
    irsend.sendRaw(durations, len, 38);
    commands_executed_counter++;
  } else {
    Serial.println("[MQTT] Failed to parse command for IR sending.");
    commands_failed_ir++;
    return; // Stop processing this command
  }

  // Update latency metrics
  unsigned long t_tx = millis();
  last_cmd_latency_ms = t_tx - t_rx;
  avg_cmd_latency_ms = (avg_cmd_latency_ms * 9 + last_cmd_latency_ms) / 10;

  ACUState currentState = remote.getState();
  bool stateChanged = memcmp(&currentState, &lastState, sizeof(ACUState)) != 0;

  if (stateChanged) {
    char timeBuffer[30];
    getTimestamp(timeBuffer, sizeof(timeBuffer));
    strncpy(lastChangeTimestamp, timeBuffer, sizeof(lastChangeTimestamp));

    // Publish updated ACU state
    StaticJsonDocument<128> stateDoc;
    remote.toJSON(stateDoc.to<JsonObject>());
    char output[128];
    serializeJson(stateDoc, output, sizeof(output));
    mqtt_client.publish(mqtt_topic_pub_state, output, true);

    // Update the stored previous state
    lastState = currentState;
  }

  // Update last command timestamp (for diagnostics)
  char timeBuffer[30];
  getTimestamp(timeBuffer, sizeof(timeBuffer));
  strncpy(lastCommandTimestamp, timeBuffer, sizeof(lastCommandTimestamp));

  // Publish diagnostics or metrics (alternate)
  static bool send_diag = true;
  if(send_diag) publishDiagnostics();
  else         publishMetrics();
  send_diag = !send_diag;

}

void processMQTTQueue() {
  while (mqttQueueHead != mqttQueueTail) {
    // Process tail
    MQTTQueueItem* item = &mqttQueue[mqttQueueTail];

    Serial.print("[MQTT] Processing topic: ");
    Serial.println(item->topic);

    if (topicMatchesModule(item->topic)) {
        handleReceivedCommand(item->topic, (byte*)item->payload, item->length);
    } else {
        Serial.println("[MQTT] Topic rejected by filter.");
    }

    // Advance tail
    mqttQueueTail = (mqttQueueTail + 1) % MQTT_QUEUE_SIZE;
    yield();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  uint8_t nextHead = (mqttQueueHead + 1) % MQTT_QUEUE_SIZE;
  if (nextHead != mqttQueueTail) {
    strncpy(mqttQueue[mqttQueueHead].topic, topic, sizeof(mqttQueue[0].topic) - 1);
    mqttQueue[mqttQueueHead].topic[sizeof(mqttQueue[0].topic) - 1] = '\0';
    
    unsigned int copyLen = (length < sizeof(mqttQueue[0].payload)) ? length : sizeof(mqttQueue[0].payload);
    memcpy(mqttQueue[mqttQueueHead].payload, payload, copyLen);
    if (copyLen < sizeof(mqttQueue[0].payload)) mqttQueue[mqttQueueHead].payload[copyLen] = '\0';
    mqttQueue[mqttQueueHead].length = copyLen;
    
    mqttQueueHead = nextHead;
  }
}


// =================================================================================
// 6. CONNECTION MANAGEMENT (SETUP & LOOP)
// =================================================================================

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
      
      mqtt_connect_ts = millis();
      prev_mqtt_status = true;

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

void setupMQTT() {
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(callback);
  mqtt_client.setKeepAlive(45); // 45 seconds
  mqtt_client.setBufferSize(512); // For identity and metrics
}

void handleMQTT() {
  updateConnectionStats();

  if (!mqtt_client.connected()) {
    mqtt_reconnect();
    yield();
  } 

  mqtt_client.loop();
  processMQTTQueue();
  yield();

  publishHeartbeat();
  yield();

}

void mqttDisconnect() {
  if (mqtt_client.connected()) {
    mqtt_client.disconnect();
  }
}
