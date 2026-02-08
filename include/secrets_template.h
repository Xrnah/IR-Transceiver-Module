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
// 1. IR SEND PIPELINE (build-time selection)
// ----------------------------------------------------------------
// 1 = use IRremoteESP8266 adapters (MHI88/MHI152)
// 0 = use raw IR modulator (based on PJA502A704AA remote)
#define USE_ACU_ADAPTER 1

// Model string reported in identity messages
// Example values: "MHI_64", "MHI_88", "MHI_152"
#if USE_ACU_ADAPTER
  #define ACU_REMOTE_MODEL "MHI_88"
#else
  #define ACU_REMOTE_MODEL "MHI_64"
#endif


// ----------------------------------------------------------------
// 2. Hidden WiFi Credentials (for hidden networks in main.cpp)
// ----------------------------------------------------------------
#define HIDDEN_SSID "sample_ssid"
#define HIDDEN_PASS "sample_pass"


// ----------------------------------------------------------------
// 3. MQTT Broker Configuration (MQTT.h)
// ----------------------------------------------------------------
#define MQTT_SERVER "192.168.1.1"
#define MQTT_PORT 1234
#define MQTT_USER "user"
#define MQTT_PASS "pass"

#define STATE_PATH   "modules"
#define CONTROL_PATH "commands"


// ----------------------------------------------------------------
// 4. Per-Module Configuration (mass production)
// ----------------------------------------------------------------
// Use short values for production, e.g., "08F", "808", "ACU1"
#define DEFINED_FLOOR "Floor_Topic"
#define DEFINED_ROOM  "Room_Topic"
#define DEFINED_UNIT  "ACU_Topic"

// Room type ID: IR=1, IL=2, CA=3, OR=4, SR=5
#define DEFINED_ROOM_TYPE_ID 1
#define DEFINED_DEPARTMENT "School of Engineering"

// ----------------------------------------------------------------
// 5. NTP Servers
// ----------------------------------------------------------------
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"


// ----------------------------------------------------------------
// 6. Hardware Versioning (optional)
// ----------------------------------------------------------------
#define DEFINED_VERSION_HASH "dev"
#define DEFINED_DEPLOYMENT_DATE "January 1, 2026"

