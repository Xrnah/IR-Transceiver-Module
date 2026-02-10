#pragma once

/*
 * MQTT.h
 *
 * Public MQTT module interface for setup, loop handling, and telemetry updates.
 */

/**
 * @brief Build MQTT topic strings for this device.
 */
void setupMQTTTopics();

/**
 * @brief Configure MQTT client (server, callbacks, buffers).
 */
void setupMQTT();

/**
 * @brief Run MQTT loop processing and publish heartbeats.
 */
void handleMQTT();

/**
 * @brief Update cached connection metrics used by telemetry.
 */
void updateConnectionStats();

/**
 * @brief Disconnect MQTT client if connected.
 */
void mqttDisconnect();

/**
 * @brief Increment WiFi disconnect counter (used by WiFiManager).
 */
void incrementWifiDisconnectCounter();
