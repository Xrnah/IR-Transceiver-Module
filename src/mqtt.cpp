#include "MQTT.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "logging.h"
#include "secrets.h"
#include "ACU_remote_encoder.h"
#if USE_ACU_ADAPTER
  #include "ACU_ir_adapters.h"
#else
  #include "ACU_IR_modulator.h"
#endif
#include <NTP.h>
#include <pgmspace.h>

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

namespace {

constexpr const char* k_log_tag = "MQTT";

// =================================================================================
// 1. CONFIGURATION & CONSTANTS
// =================================================================================

// ─────────────────────────────────────────────
// 🔧 MQTT Broker Info
// ─────────────────────────────────────────────
// Broker address, user, and password are defined in 'secrets.h'
const char* g_mqtt_server = MQTT_SERVER;
const int g_mqtt_port = MQTT_PORT;

const char* g_mqtt_user = MQTT_USER;
const char* g_mqtt_pass = MQTT_PASS;

// ─────────────────────────────────────────────
// 🧩 Topic Components (custom per device)
// ─────────────────────────────────────────────
// format: "floor_id/room_id/unit_id"
const char* g_state_root = STATE_PATH;
const char* g_control_root = CONTROL_PATH;
const char* g_floor_id = DEFINED_FLOOR;
const char* g_room_id = DEFINED_ROOM;
const char* g_unit_id = DEFINED_UNIT;

// ─────────────────────────────────────────────
// ⚙️ MQTT Settings
// ─────────────────────────────────────────────
const char g_lwt_message_json[] PROGMEM = "{\"status\":\"offline\"}";
constexpr uint8_t g_mqtt_qos = 1; // Quality of Service
constexpr bool g_is_clean_session = false;
constexpr unsigned long g_heartbeat_interval_ms = 15000; // 15 seconds
constexpr unsigned long g_metrics_interval_ms = 120000;  // 120 seconds
constexpr unsigned int g_mqtt_keepalive_s = 45;
constexpr unsigned int g_mqtt_buffer_size = 512;

// =================================================================================
// 2. GLOBAL OBJECTS & STATE
// =================================================================================

// ─────────────────────────────────────────────
// 📦 Clients & Instances
// ─────────────────────────────────────────────
WiFiClient g_esp_client;
PubSubClient g_mqtt_client(g_esp_client);
ACURemote g_acu_remote("MITSUBISHI_HEAVY_64");
#if USE_ACU_ADAPTER
  // IR adapter selection (switch to Mhi152Adapter if needed)
  Mhi88Adapter g_acu_adapter;
#endif
// ─────────────────────────────────────────────
// 📝 Buffers
// ─────────────────────────────────────────────
// MQTT topic buffers
char g_mqtt_topic_sub_unit[64];
char g_mqtt_topic_pub_state[80];
char g_mqtt_topic_pub_identity[80];
char g_mqtt_topic_pub_deployment[80];
char g_mqtt_topic_pub_diagnostics[80];
char g_mqtt_topic_pub_metrics[80];
char g_mqtt_topic_pub_error[80];

// MQTT Queue for ISR-safe decoupling
struct MQTTQueueItem {
  char topic[64];
  char payload[256];
  unsigned int length;
};
constexpr uint8_t g_mqtt_queue_size = 5;
MQTTQueueItem g_mqtt_queue[g_mqtt_queue_size];
volatile uint8_t g_mqtt_queue_head = 0;
volatile uint8_t g_mqtt_queue_tail = 0;

// Heartbeat & Timestamp buffers
char g_last_received_command_json[256] = {0};
char g_last_command_timestamp[30] = {0};
char g_last_change_timestamp[30] = {0};
unsigned long g_last_heartbeat_time = 0;
unsigned long g_last_metrics_time = 0;

bool g_is_mqtt_publish_in_progress = false; // lock to prevent overlapping publishes

uint32_t g_last_state_crc = 0;
bool g_is_state_initialized = false;
char g_lwt_message[sizeof(g_lwt_message_json)];

static ACUState g_last_state = {0};

static StaticJsonDocument<256> g_deployment_doc;
static StaticJsonDocument<160> g_diag_doc;
static StaticJsonDocument<384> g_metrics_doc;
static StaticJsonDocument<192> g_state_pub_doc;

// Pre-allocated serialization buffers
static char g_deployment_output[224];
static char g_diag_output[192];
static char g_metrics_output[384];
static char g_state_pub_output[192];

// ─────────────────────────────────────────────
// 📊 Metrics & Counters
// ─────────────────────────────────────────────
// Connection Stats
unsigned long g_wifi_connect_ts = 0;
unsigned long g_mqtt_connect_ts = 0;
unsigned int g_wifi_disconnect_counter = 0;
unsigned int g_mqtt_disconnect_counter = 0;
unsigned int g_commands_received_counter = 0;
unsigned int g_commands_executed_counter = 0;
bool g_is_prev_wifi_status = false;
bool g_is_prev_mqtt_status = false;

// Cumulative availability counters
uint32_t g_wifi_connected_total_s = 0;
uint32_t g_mqtt_connected_total_s = 0;
unsigned long g_last_wifi_update_ms = 0;
unsigned long g_last_mqtt_update_ms = 0;

// Command latency metrics
uint32_t g_last_cmd_latency_ms = 0;
uint32_t g_avg_cmd_latency_ms = 0;

// Command failure counters
uint32_t g_commands_failed_parse = 0;
uint32_t g_commands_failed_struct = 0;
uint32_t g_commands_failed_ir = 0;

// MQTT publish failures
uint32_t g_mqtt_publish_failures = 0;

// Uptime wrap tracking (millis() wraps ~49.7 days)
uint32_t g_uptime_wraps = 0;
unsigned long g_last_uptime_ms = 0;

// Cached metric snapshots (computed in updateConnectionStats)
uint64_t g_uptime_s_cached = 0;
uint32_t g_wifi_uptime_s_cached = 0;
uint32_t g_mqtt_uptime_s_cached = 0;
int32_t g_wifi_rssi_cached = -127;
uint32_t g_free_heap_cached = 0;
uint32_t g_heap_frag_cached = 0;

// =================================================================================
// 3. INTERNAL UTILITIES
// =================================================================================

void publishMqttErrorContext(const char* error, const char* topic, const uint8_t* payload, unsigned int length, int rc);

bool isTopicMatchingModule(char* topic) {
  return strcmp(topic, g_mqtt_topic_sub_unit) == 0;
}

// =================================================================================
// 4. PUBLISHING FUNCTIONS
// =================================================================================
void publishACUState(const JsonObject& state_obj) {
  if (!g_mqtt_client.connected()) {
    logDebug(k_log_tag, "Not connected, skipping publish.");
    return;
  }

  g_state_pub_doc.clear();

  // Map internal keys to schema
  if (state_obj.containsKey("temperature")) g_state_pub_doc["temperature"] = state_obj["temperature"];
  if (state_obj.containsKey("fan_speed"))    g_state_pub_doc["fan_speed"]    = state_obj["fan_speed"];
  if (state_obj.containsKey("mode"))        g_state_pub_doc["mode"]        = state_obj["mode"];
  if (state_obj.containsKey("louver"))      g_state_pub_doc["louver"]      = state_obj["louver"];
  if (state_obj.containsKey("power"))        g_state_pub_doc["power"]        = state_obj["power"];

  if (g_last_change_timestamp[0] != '\0') {
    g_state_pub_doc["last_change_ts"] = g_last_change_timestamp;
  }

  size_t len = serializeJson(g_state_pub_doc, g_state_pub_output, sizeof(g_state_pub_output));

  bool is_ok = g_mqtt_client.publish(g_mqtt_topic_pub_state, (const uint8_t*)g_state_pub_output, len, true); // retain = true
  if (is_ok) {
    logInfo(k_log_tag, "Published state: %s", g_state_pub_output);
  } else {
    logError(k_log_tag, "Publish failed.");
    publishMqttErrorContext("publish_failed", g_mqtt_topic_pub_state, (const uint8_t*)g_state_pub_output, len, 0);
    g_mqtt_publish_failures++;
  }
}

void publishIdentity() {
  if (!g_mqtt_client.connected()) return;
  StaticJsonDocument<512> doc;
  char client_id_str[32];
  snprintf(client_id_str, sizeof(client_id_str), "ESP8266Client-%06X", ESP.getChipId());
  
  doc["device_id"] = client_id_str;
  doc["mac_address"] = WiFi.macAddress();
  doc["acu_remote_model"] = ACU_REMOTE_MODEL; 
  // doc["room_type"] = DEFINED_ROOM_TYPE;
  doc["room_type_id"] = DEFINED_ROOM_TYPE_ID;
  doc["department"] = DEFINED_DEPARTMENT;

  char output[384];
  if (doc.overflowed()) logWarn(k_log_tag, "Identity JSON doc overflow");
  size_t n = serializeJson(doc, output, sizeof(output));
  if (n >= sizeof(output)) logWarn(k_log_tag, "Identity output truncated");

  g_mqtt_client.publish(g_mqtt_topic_pub_identity, output, true); // Retain identity info
  doc.clear();
}

void publishDeployment() {
  if (!g_mqtt_client.connected()) return;
  g_deployment_doc.clear();
  g_deployment_doc["ip_address"] = WiFi.localIP().toString();
  g_deployment_doc["version_hash"] = GIT_HASH;
  g_deployment_doc["build_timestamp"] = BUILD_TIMESTAMP;

  // g_deployment_doc["build_host"] = BUILD_HOST;
  // g_deployment_doc["build_user"] = BUILD_USER;
  // g_deployment_doc["deployment_date"] = DEFINED_DEPLOYMENT_DATE;
  // g_deployment_doc["version_hash"] = DEFINED_VERSION_HASH;

  g_deployment_doc["reset_reason"] = ESP.getResetReason();

  if (g_deployment_doc.overflowed()) logWarn(k_log_tag, "Deployment JSON doc overflow");
  size_t n = serializeJson(g_deployment_doc, g_deployment_output, sizeof(g_deployment_output));
  if (n >= sizeof(g_deployment_output)) logWarn(k_log_tag, "Deployment output truncated");
  g_mqtt_client.publish(g_mqtt_topic_pub_deployment, g_deployment_output, true); // Retain deployment info

  g_deployment_doc.clear();
}

void publishDiagnostics() {
  if (!g_mqtt_client.connected()) return;

  // Check lock
  if (g_is_mqtt_publish_in_progress) return;
  g_is_mqtt_publish_in_progress = true;

  g_diag_doc["status"] = "online";

  char time_buffer[30];
  getTimestamp(time_buffer, sizeof(time_buffer));
  g_diag_doc["last_seen_ts"] = time_buffer;

  if (g_last_command_timestamp[0] != '\0') {
    g_diag_doc["last_cmd_ts"] = g_last_command_timestamp;
  }

  g_diag_doc["wifi_rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  g_diag_doc["free_heap"] = ESP.getFreeHeap();


  // Serialize into pre-allocated global buffer
  size_t n = serializeJson(g_diag_doc, g_diag_output, sizeof(g_diag_output));

  bool is_ok = g_mqtt_client.publish(
    g_mqtt_topic_pub_diagnostics,
    (const uint8_t*)g_diag_output,
    n,
    false // no retain
  );

  if (!is_ok) g_mqtt_publish_failures++;

  g_is_mqtt_publish_in_progress = false;
  g_diag_doc.clear();
}

void publishMetrics() {
  if (!g_mqtt_client.connected()) return;

  if (g_is_mqtt_publish_in_progress) return;
  g_is_mqtt_publish_in_progress = true;

  g_metrics_doc["uptime_s"] = g_uptime_s_cached;
  g_metrics_doc["wifi_uptime_s"] = g_wifi_uptime_s_cached;
  g_metrics_doc["mqtt_uptime_s"] = g_mqtt_uptime_s_cached;

  g_metrics_doc["wifi_conn_total_s"] = g_wifi_connected_total_s;
  g_metrics_doc["mqtt_conn_total_s"] = g_mqtt_connected_total_s;

  g_metrics_doc["wifi_disc"] = g_wifi_disconnect_counter;
  g_metrics_doc["mqtt_disc"] = g_mqtt_disconnect_counter;
  g_metrics_doc["cmd_rx"] = g_commands_received_counter;
  g_metrics_doc["cmd_exec"] = g_commands_executed_counter;
  g_metrics_doc["cmd_fail_parse"] = g_commands_failed_parse;
  g_metrics_doc["cmd_fail_struct"] = g_commands_failed_struct;
  g_metrics_doc["cmd_fail_ir"] = g_commands_failed_ir;
  g_metrics_doc["cmd_latency_ms"] = g_last_cmd_latency_ms;
  g_metrics_doc["cmd_latency_avg_ms"] = g_avg_cmd_latency_ms;

  g_metrics_doc["free_heap"] = g_free_heap_cached;
  g_metrics_doc["heap_frag"] = g_heap_frag_cached;
  g_metrics_doc["mqtt_pub_fail"] = g_mqtt_publish_failures;

  // Serialize into pre-allocated global buffer
  size_t n = serializeJson(g_metrics_doc, g_metrics_output, sizeof(g_metrics_output));

  bool is_ok = g_mqtt_client.publish(
    g_mqtt_topic_pub_metrics,
    (const uint8_t*)g_metrics_output,
    n,
    false
  );

  if (!is_ok) g_mqtt_publish_failures++;

  g_is_mqtt_publish_in_progress = false;
  g_metrics_doc.clear();

  // Optional debug
  logDebug(k_log_tag, "Metrics published: %s", g_metrics_output);
}

void publishMqttErrorContext(const char* error, const char* topic, const uint8_t* payload, unsigned int length, int rc) {
#if LOG_LEVEL >= 3
  const char* error_str = (error != nullptr) ? error : "unknown_error";
  const char* topic_str = (topic != nullptr) ? topic : "n/a";
  logError(k_log_tag, "Error context: %s (topic=%s rc=%d len=%u)", error_str, topic_str, rc, length);

  if (!g_mqtt_client.connected()) return;

  StaticJsonDocument<384> error_doc;
  char time_buffer[30];
  getTimestamp(time_buffer, sizeof(time_buffer));
  error_doc["ts"] = time_buffer;
  error_doc["error"] = error_str;
  error_doc["broker"] = g_mqtt_server;
  error_doc["port"] = g_mqtt_port;
  if (topic != nullptr) error_doc["topic"] = topic;
  if (rc != 0) error_doc["rc"] = rc;

  if (payload != nullptr && length > 0) {
    constexpr size_t k_payload_max = 128;
    char payload_buf[k_payload_max];
    size_t copy_len = (length < (k_payload_max - 1)) ? length : (k_payload_max - 1);
    memcpy(payload_buf, payload, copy_len);
    payload_buf[copy_len] = '\0';
    error_doc["payload_len"] = length;
    error_doc["payload"] = payload_buf;
  }

  char output[384];
  size_t n = serializeJson(error_doc, output, sizeof(output));
  bool is_ok = g_mqtt_client.publish(
    g_mqtt_topic_pub_error,
    (const uint8_t*)output,
    n,
    false
  );
  if (!is_ok) g_mqtt_publish_failures++;
#else
  (void)error;
  (void)topic;
  (void)payload;
  (void)length;
  (void)rc;
#endif
}


void publishOnReconnect() {
  publishIdentity();
  publishDeployment();
  publishDiagnostics();
  publishMetrics();

  // Republish last known state if available
  if (g_last_received_command_json[0] != '\0') {
    StaticJsonDocument<384> doc;
    deserializeJson(doc, g_last_received_command_json);
    publishACUState(doc.as<JsonObject>());
  }
}

// Publish heartbeat on connection
void publishHeartbeat() {
  if (!g_mqtt_client.connected()) return;

  if ((long)(millis() - g_last_heartbeat_time) >= g_heartbeat_interval_ms) {
    publishDiagnostics();
    g_last_heartbeat_time = millis();
  }

  if ((long)(millis() - g_last_metrics_time) >= g_metrics_interval_ms) {
    publishMetrics();
    g_last_metrics_time = millis();
  }
}


// =================================================================================
// 5. COMMAND HANDLING
// =================================================================================

void handleReceivedCommand(char* topic, byte* payload, unsigned int length) {
  unsigned long rx_time_ms = millis();

  // Deserialize incoming JSON
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    logError(k_log_tag, "JSON parse failed: %s", err.c_str());
    publishMqttErrorContext("json_parse_failed", topic, payload, length, 0);
    g_commands_failed_parse++;
    return;
  }

  g_commands_received_counter++;

  // Handle potential nested "state" object
  JsonObjectConst state_obj = doc.containsKey("state") ? doc["state"] : doc.as<JsonObjectConst>();

  if (!g_acu_remote.fromJSON(state_obj)) {
    logError(k_log_tag, "Invalid command structure.");
    publishMqttErrorContext("invalid_command_structure", topic, payload, length, 0);
    g_commands_failed_struct++;
    return;
  }

#if USE_ACU_ADAPTER
  logDebug(k_log_tag, "JSON parsed. Adapter: %s", g_acu_adapter.name());
#else
  logDebug(k_log_tag, "JSON parsed. Using legacy IR modulator.");
#endif

  yield(); // Allow ESP8266 background tasks

#if USE_ACU_ADAPTER
  // Send IR using adapter
  if (g_acu_adapter.send(g_acu_remote.getState())) {
    g_commands_executed_counter++;
  } else {
    logError(k_log_tag, "Failed to send IR command.");
    publishMqttErrorContext("ir_send_failed", topic, payload, length, 0);
    g_commands_failed_ir++;
    return; // Stop processing this command
  }
#else
  // Legacy IR modulator path
  uint64_t command = g_acu_remote.encodeCommand();
  size_t len = 0;
  if (parseBinaryToDurations(command, g_durations, len)) {
    g_ir_send.sendRaw(g_durations, len, 38);
    g_commands_executed_counter++;
  } else {
    logError(k_log_tag, "Failed to parse command for IR sending.");
    publishMqttErrorContext("ir_parse_failed", topic, payload, length, 0);
    g_commands_failed_ir++;
    return; // Stop processing this command
  }
#endif

  // Update latency metrics
  unsigned long tx_time_ms = millis();
  g_last_cmd_latency_ms = tx_time_ms - rx_time_ms;
  g_avg_cmd_latency_ms = (g_avg_cmd_latency_ms * 9 + g_last_cmd_latency_ms) / 10;

  ACUState current_state = g_acu_remote.getState();
  bool is_state_changed = memcmp(&current_state, &g_last_state, sizeof(ACUState)) != 0;

  if (is_state_changed) {
    char time_buffer[30];
    getTimestamp(time_buffer, sizeof(time_buffer));
    strncpy(g_last_change_timestamp, time_buffer, sizeof(g_last_change_timestamp));

    // Publish updated ACU state
    StaticJsonDocument<128> temp_state_doc;
    g_acu_remote.toJSON(temp_state_doc.to<JsonObject>());
    publishACUState(temp_state_doc.as<JsonObject>());

    // Update the stored previous state
    g_last_state = current_state;
  }

  // Update last command timestamp (for diagnostics)
  char time_buffer[30];
  getTimestamp(time_buffer, sizeof(time_buffer));
  strncpy(g_last_command_timestamp, time_buffer, sizeof(g_last_command_timestamp));

  // Publish diagnostics or metrics (alternate)
  static bool is_send_diag = true;
  if (is_send_diag) publishDiagnostics();
  else             publishMetrics();
  is_send_diag = !is_send_diag;

}

void processMQTTQueue() {
  while (g_mqtt_queue_head != g_mqtt_queue_tail) {
    // Process tail
    MQTTQueueItem* item = &g_mqtt_queue[g_mqtt_queue_tail];

    logDebug(k_log_tag, "Processing topic: %s", item->topic);

    if (isTopicMatchingModule(item->topic)) {
      handleReceivedCommand(item->topic, (byte*)item->payload, item->length);
    } else {
      logDebug(k_log_tag, "Topic rejected by filter.");
    }

    // Advance tail
    g_mqtt_queue_tail = (g_mqtt_queue_tail + 1) % g_mqtt_queue_size;
    yield();
  }
}

void handleMQTTCallback(char* topic, byte* payload, unsigned int length) {
  uint8_t next_head = (g_mqtt_queue_head + 1) % g_mqtt_queue_size;
  if (next_head != g_mqtt_queue_tail) {
    strncpy(g_mqtt_queue[g_mqtt_queue_head].topic, topic, sizeof(g_mqtt_queue[0].topic) - 1);
    g_mqtt_queue[g_mqtt_queue_head].topic[sizeof(g_mqtt_queue[0].topic) - 1] = '\0';
    
    unsigned int copy_len = (length < sizeof(g_mqtt_queue[0].payload)) ? length : sizeof(g_mqtt_queue[0].payload);
    memcpy(g_mqtt_queue[g_mqtt_queue_head].payload, payload, copy_len);
    if (copy_len < sizeof(g_mqtt_queue[0].payload)) g_mqtt_queue[g_mqtt_queue_head].payload[copy_len] = '\0';
    g_mqtt_queue[g_mqtt_queue_head].length = copy_len;
    
    g_mqtt_queue_head = next_head;
  }
}


// =================================================================================
// 6. CONNECTION MANAGEMENT (SETUP & LOOP)
// =================================================================================

void reconnectMQTT() {
  static unsigned long last_attempt_ms = 0;
  constexpr unsigned long retry_interval_ms = 10000;

  if (g_mqtt_client.connected()) return;

  unsigned long now_ms = millis();
  if (now_ms - last_attempt_ms >= retry_interval_ms) {
    last_attempt_ms = now_ms;

    logInfo(k_log_tag, "Connecting...");
    
    static char client_id[32];
    static bool is_id_init = false;

    if (!is_id_init) {
      snprintf(client_id, sizeof(client_id),
               "ESP8266Client-%06X", ESP.getChipId());
      // Copy LWT message from PROGMEM to RAM buffer
      strcpy_P(g_lwt_message, g_lwt_message_json);
      is_id_init = true;
    }

    if (g_mqtt_client.connect(client_id, g_mqtt_user, g_mqtt_pass, g_mqtt_topic_pub_diagnostics, g_mqtt_qos, true, g_lwt_message, g_is_clean_session)) {
      logInfo(k_log_tag, "Connected.");
      
      g_mqtt_connect_ts = millis();
      g_is_prev_mqtt_status = true;

      // g_mqtt_client.subscribe(g_mqtt_topic_sub_floor, g_mqtt_qos);
      // g_mqtt_client.subscribe(g_mqtt_topic_sub_room, g_mqtt_qos);
      g_mqtt_client.subscribe(g_mqtt_topic_sub_unit, g_mqtt_qos);
      publishOnReconnect();
    } else {
      int rc = g_mqtt_client.state();
      logError(k_log_tag, "Connect failed (rc=%d), retrying...", rc);
      publishMqttErrorContext("connect_failed", nullptr, nullptr, 0, rc);
    }
  }
}

} // namespace

void setupMQTTTopics() {
  snprintf(g_mqtt_topic_sub_unit,        sizeof(g_mqtt_topic_sub_unit),        "%s/%s/%s/%s",            g_control_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_state,       sizeof(g_mqtt_topic_pub_state),       "%s/%s/%s/%s/state",      g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_identity,    sizeof(g_mqtt_topic_pub_identity),    "%s/%s/%s/%s/identity",   g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_deployment,  sizeof(g_mqtt_topic_pub_deployment),  "%s/%s/%s/%s/deployment", g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_diagnostics, sizeof(g_mqtt_topic_pub_diagnostics), "%s/%s/%s/%s/diagnostics", g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_metrics,     sizeof(g_mqtt_topic_pub_metrics),     "%s/%s/%s/%s/metrics",    g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_error,       sizeof(g_mqtt_topic_pub_error),       "%s/%s/%s/%s/error",      g_state_root, g_floor_id, g_room_id, g_unit_id);
}

void updateConnectionStats() {
  unsigned long now_ms = millis();

  // Track millis() wrap to extend uptime range
  if (g_last_uptime_ms != 0 && now_ms < g_last_uptime_ms) {
    g_uptime_wraps++;
  }
  g_last_uptime_ms = now_ms;

  // Snapshot uptime (seconds) from extended millisecond counter
  uint64_t total_ms = ((uint64_t)g_uptime_wraps << 32) + (uint64_t)now_ms;
  g_uptime_s_cached = total_ms / 1000ULL;

  // WiFi Stats
  bool is_current_wifi = (WiFi.status() == WL_CONNECTED);
  if (is_current_wifi) {
    if (g_last_wifi_update_ms == 0) {
      g_last_wifi_update_ms = now_ms;
    } else if (now_ms - g_last_wifi_update_ms >= 1000) {
      g_wifi_connected_total_s += (now_ms - g_last_wifi_update_ms) / 1000;
      g_last_wifi_update_ms = now_ms;
    }
  } else {
    g_last_wifi_update_ms = 0;
  }

  if (is_current_wifi && !g_is_prev_wifi_status) {
    g_wifi_connect_ts = now_ms;
  } 
  else if (!is_current_wifi && g_is_prev_wifi_status) {
    g_wifi_disconnect_counter++;
  }
  g_is_prev_wifi_status = is_current_wifi;

  // MQTT Stats
  bool is_current_mqtt = g_mqtt_client.connected();
  if (is_current_mqtt) {
    if (g_last_mqtt_update_ms == 0) {
      g_last_mqtt_update_ms = now_ms;
    } else if (now_ms - g_last_mqtt_update_ms >= 1000) {
      g_mqtt_connected_total_s += (now_ms - g_last_mqtt_update_ms) / 1000;
      g_last_mqtt_update_ms = now_ms;
    }
  } else {
    g_last_mqtt_update_ms = 0;
  }

  if (is_current_mqtt && !g_is_prev_mqtt_status) {
    g_mqtt_connect_ts = now_ms;
  } 
  else if (!is_current_mqtt && g_is_prev_mqtt_status) {
    g_mqtt_disconnect_counter++;
  }
  g_is_prev_mqtt_status = is_current_mqtt;

  // Snapshot derived metrics
  g_wifi_uptime_s_cached = is_current_wifi ? (now_ms - g_wifi_connect_ts) / 1000 : 0;
  g_mqtt_uptime_s_cached = is_current_mqtt ? (now_ms - g_mqtt_connect_ts) / 1000 : 0;
  g_wifi_rssi_cached = is_current_wifi ? WiFi.RSSI() : -127;
  g_free_heap_cached = ESP.getFreeHeap();
  g_heap_frag_cached = ESP.getHeapFragmentation();
}

void setupMQTT() {
  g_mqtt_client.setServer(g_mqtt_server, g_mqtt_port);
  g_mqtt_client.setCallback(handleMQTTCallback);
  g_mqtt_client.setKeepAlive(g_mqtt_keepalive_s); // seconds
  g_mqtt_client.setBufferSize(g_mqtt_buffer_size); // For identity and metrics
#if USE_ACU_ADAPTER
  g_acu_adapter.begin();
#endif
}

void handleMQTT() {
  if (!g_mqtt_client.connected()) {
    reconnectMQTT();
    yield();
  } 

  g_mqtt_client.loop();
  processMQTTQueue();
  yield();

  publishHeartbeat();
  yield();

}

void mqttDisconnect() {
  if (g_mqtt_client.connected()) {
    g_mqtt_client.disconnect();
  }
}

void incrementWifiDisconnectCounter() {
  g_wifi_disconnect_counter++;
}
