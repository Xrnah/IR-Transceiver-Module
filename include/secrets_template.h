#pragma once

/*
 ************************************************************************************
 *                                                                                  *
 *    ** IMPORTANT: DO NOT commit your secrets! **                                  *
 *                                                                                  *
 *    To use this, copy this file to 'secrets.h' and fill in your actual            *
 *    credentials. The 'secrets.h' file is ignored by Git.                          *
 *                                                                                  *
 ************************************************************************************
 */


// ----------------------------------------------------------------
// 1. Hardcoded WiFi Credentials (for hidden networks in main.cpp)
// ----------------------------------------------------------------
#define HIDDEN_SSID "sample_ssid"
#define HIDDEN_PASS "sample_pass"


// ----------------------------------------------------------------
// 2. MQTT Broker Configuration (MQTT.h)
// ----------------------------------------------------------------
#define MQTT_SERVER "192.168.1.1"
#define MQTT_PORT 1234
#define MQTT_USER "user"
#define MQTT_PASS "pass"

  const char* floor_id = "Floor_Topic";
  const char* room_id  = "Room_Topic";
  const char* unit_id  = "ACU_Topic";

// ----------------------------------------------------------------
// 3. Known WiFi Networks Table (WiFiManager.cpp)
// ----------------------------------------------------------------

