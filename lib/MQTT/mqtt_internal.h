#pragma once

#include <Arduino.h>

#if !defined(ARDUINO_ARCH_ESP8266)
#error "ESP8266 only"
#endif

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <pgmspace.h>

#include "logging.h"
#include "secrets.h"
#include "ACU_remote_encoder.h"
#if USE_ACU_ADAPTER
  #include "ACU_ir_adapters.h"
#else
  #include "ACU_IR_modulator.h"
#endif
#include <NTP.h>

// =================================================================================
// 0. SAFE DEFAULTS (avoid build errors if macros are missing)
// =================================================================================
#ifndef MQTT_SERVER
  #define MQTT_SERVER "127.0.0.1"
#endif
#ifndef MQTT_PORT
  #define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
  #define MQTT_USER ""
#endif
#ifndef MQTT_PASS
  #define MQTT_PASS ""
#endif
#ifndef STATE_PATH
  #define STATE_PATH "state"
#endif
#ifndef CONTROL_PATH
  #define CONTROL_PATH "control"
#endif
#ifndef DEFINED_FLOOR
  #define DEFINED_FLOOR "unknown_floor"
#endif
#ifndef DEFINED_ROOM
  #define DEFINED_ROOM "unknown_room"
#endif
#ifndef DEFINED_UNIT
  #define DEFINED_UNIT "unknown_unit"
#endif
#ifndef DEFINED_ROOM_TYPE_ID
  #define DEFINED_ROOM_TYPE_ID 0
#endif
#ifndef DEFINED_DEPARTMENT
  #define DEFINED_DEPARTMENT "unknown_department"
#endif
#ifndef ACU_REMOTE_MODEL
  #define ACU_REMOTE_MODEL "unknown_model"
#endif
#ifndef GIT_HASH
  #define GIT_HASH "unknown"
#endif
#ifndef BUILD_TIMESTAMP
  #define BUILD_TIMESTAMP "unknown"
#endif
#ifndef BUILD_HOST
  #define BUILD_HOST "unknown"
#endif
#ifndef BUILD_USER
  #define BUILD_USER "unknown"
#endif

constexpr const char* k_log_tag = "MQTT";
constexpr size_t k_error_payload_max = 128;
constexpr size_t k_error_payload_buf = k_error_payload_max + 1;
constexpr size_t k_error_str_max = 32;
constexpr size_t k_error_topic_max = 64;
constexpr size_t k_lwt_message_len = sizeof("{\"status\":\"offline\"}");

constexpr uint8_t g_mqtt_qos = 1; // Quality of Service
constexpr bool g_is_clean_session = false;
constexpr unsigned long g_heartbeat_interval_ms = 15000; // 15 seconds
constexpr unsigned long g_metrics_interval_ms = 120000;  // 120 seconds
constexpr unsigned int g_mqtt_keepalive_s = 45;
constexpr unsigned int g_mqtt_buffer_size = 512;
constexpr uint8_t g_mqtt_queue_size = 5;

struct ErrorContextSnapshot {
  bool has_data = false;
  char error[k_error_str_max] = {0};
  char topic[k_error_topic_max] = {0};
  char payload[k_error_payload_buf] = {0};
  unsigned int payload_len = 0;
  int rc = 0;
  bool has_payload = false;
};

struct MQTTQueueItem {
  char topic[64];
  char payload[256];
  unsigned int length;
};

extern ErrorContextSnapshot g_last_error_ctx;
extern bool g_has_queued_error_ctx;

extern WiFiClient g_esp_client;
extern PubSubClient g_mqtt_client;
extern ACURemote g_acu_remote;
#if USE_ACU_ADAPTER
extern MHI88Adapter g_acu_adapter;
#endif

extern const char* g_mqtt_server;
extern const int g_mqtt_port;
extern const char* g_mqtt_user;
extern const char* g_mqtt_pass;

extern const char* g_state_root;
extern const char* g_control_root;
extern const char* g_floor_id;
extern const char* g_room_id;
extern const char* g_unit_id;

extern const char g_lwt_message_json[] PROGMEM;

extern char g_mqtt_topic_sub_unit[64];
extern char g_mqtt_topic_pub_state[80];
extern char g_mqtt_topic_pub_identity[80];
extern char g_mqtt_topic_pub_deployment[80];
extern char g_mqtt_topic_pub_diagnostics[80];
extern char g_mqtt_topic_pub_metrics[80];
extern char g_mqtt_topic_pub_error[80];

extern MQTTQueueItem g_mqtt_queue[g_mqtt_queue_size];
extern volatile uint8_t g_mqtt_queue_head;
extern volatile uint8_t g_mqtt_queue_tail;

extern char g_last_received_command_json[256];
extern char g_last_command_timestamp[30];
extern char g_last_change_timestamp[30];
extern unsigned long g_last_heartbeat_time;
extern unsigned long g_last_metrics_time;

extern bool g_is_mqtt_publish_in_progress;

extern uint32_t g_last_state_crc;
extern bool g_is_state_initialized;
extern char g_lwt_message[k_lwt_message_len];

extern ACUState g_last_state;

extern StaticJsonDocument<256> g_deployment_doc;
extern StaticJsonDocument<160> g_diag_doc;
extern StaticJsonDocument<384> g_metrics_doc;
extern StaticJsonDocument<192> g_state_pub_doc;

extern char g_deployment_output[224];
extern char g_diag_output[192];
extern char g_metrics_output[384];
extern char g_state_pub_output[192];

extern StaticJsonDocument<512> g_identity_doc;
extern char g_identity_output[384];
extern StaticJsonDocument<384> g_error_ctx_doc;
extern char g_error_ctx_output[384];
extern StaticJsonDocument<256> g_rx_doc;
extern StaticJsonDocument<128> g_temp_state_doc;

extern unsigned long g_wifi_connect_ts;
extern unsigned long g_mqtt_connect_ts;
extern unsigned int g_wifi_disconnect_counter;
extern unsigned int g_mqtt_disconnect_counter;
extern unsigned int g_commands_received_counter;
extern unsigned int g_commands_executed_counter;
extern bool g_is_prev_wifi_status;
extern bool g_is_prev_mqtt_status;

extern uint32_t g_wifi_connected_total_s;
extern uint32_t g_mqtt_connected_total_s;
extern unsigned long g_last_wifi_update_ms;
extern unsigned long g_last_mqtt_update_ms;

extern uint32_t g_last_cmd_latency_ms;
extern uint32_t g_avg_cmd_latency_ms;

extern uint32_t g_commands_failed_parse;
extern uint32_t g_commands_failed_struct;
extern uint32_t g_commands_failed_ir;

extern uint32_t g_mqtt_publish_failures;

extern uint32_t g_uptime_wraps;
extern unsigned long g_last_uptime_ms;

extern uint64_t g_uptime_s_cached;
extern uint32_t g_wifi_uptime_s_cached;
extern uint32_t g_mqtt_uptime_s_cached;
extern int32_t g_wifi_rssi_cached;
extern uint32_t g_free_heap_cached;
extern uint32_t g_heap_frag_cached;

void publishMQTTErrorContext(const char* error, const char* topic, const uint8_t* payload, unsigned int length, int rc);
bool publishErrorContextSnapshot(const ErrorContextSnapshot& snapshot);
void queueErrorContextSnapshot(const ErrorContextSnapshot& snapshot);
void publishQueuedErrorContextIfAny();

void publishACUState(const JsonObject& state_obj);
void publishIdentity();
void publishDeployment();
void publishDiagnostics();
void publishMetrics();
void publishOnReconnect();
void publishHeartbeat();

void handleReceivedCommand(char* topic, byte* payload, unsigned int length);
void processMQTTQueue();
void handleMQTTCallback(char* topic, byte* payload, unsigned int length);
bool isTopicMatchingModule(char* topic);

void reconnectMQTT();
