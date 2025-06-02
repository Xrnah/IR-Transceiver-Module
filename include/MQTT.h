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
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_topic_sub = "acu/control/ACU_identifier";

WiFiClient espClient;            // Wi-Fi client for MQTT
PubSubClient client(espClient);  // MQTT client instance

// ACU remote instance initialized with signature for encoding
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

// ====== MQTT reconnect logic ======
void reconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Attempting to connect...");

    String clientId = "ESP8266Client-" + String(ESP.getChipId());  // Unique client ID

    if (client.connect(clientId.c_str())) {
      Serial.println(" connected");
      client.subscribe(mqtt_topic_sub);  // Subscribe to control topic
      Serial.print("[MQTT] Subscribed to topic: ");
      Serial.println(mqtt_topic_sub);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// ====== Initialize MQTT client ======
void setupMQTT() {
  client.setServer(mqtt_server, PORT#);  // Set broker and port
  client.setCallback(callback);          // Set message callback
}

// ====== MQTT loop to be called in main loop() ======
void handleMQTT() {
  if (!client.connected()) {
    reconnect();  // Reconnect if disconnected
  }
  client.loop();  // Process incoming messages
}
