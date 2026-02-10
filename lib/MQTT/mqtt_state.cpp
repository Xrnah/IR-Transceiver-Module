#include "mqtt_internal.h"

#if !defined(ARDUINO_ARCH_ESP8266)
#error "ESP8266 only"
#endif

ErrorContextSnapshot g_last_error_ctx;
bool g_has_queued_error_ctx = false;

// =================================================================================
// 1. CONFIGURATION & CONSTANTS
// =================================================================================

const char* g_mqtt_server = MQTT_SERVER;
extern const int g_mqtt_port = MQTT_PORT;

const char* g_mqtt_user = MQTT_USER;
const char* g_mqtt_pass = MQTT_PASS;

// format: "floor_id/room_id/unit_id"
const char* g_state_root = STATE_PATH;
const char* g_control_root = CONTROL_PATH;
const char* g_floor_id = DEFINED_FLOOR;
const char* g_room_id = DEFINED_ROOM;
const char* g_unit_id = DEFINED_UNIT;

extern const char g_lwt_message_json[] PROGMEM = "{\"status\":\"offline\"}";

// =================================================================================
// 2. GLOBAL OBJECTS & STATE
// =================================================================================

WiFiClient g_esp_client;
PubSubClient g_mqtt_client(g_esp_client);
ACURemote g_acu_remote(ACURemoteSignature::MitsubishiHeavy64);
#if USE_ACU_ADAPTER
  // IR adapter selection (switch to MHI152Adapter if needed)
  MHI88Adapter g_acu_adapter;
#endif

// MQTT topic buffers
char g_mqtt_topic_sub_unit[64];
char g_mqtt_topic_pub_state[80];
char g_mqtt_topic_pub_identity[80];
char g_mqtt_topic_pub_deployment[80];
char g_mqtt_topic_pub_diagnostics[80];
char g_mqtt_topic_pub_metrics[80];
char g_mqtt_topic_pub_error[80];

// MQTT Queue for ISR-safe decoupling
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
char g_lwt_message[k_lwt_message_len];

ACUState g_last_state = {0};

StaticJsonDocument<256> g_deployment_doc;
StaticJsonDocument<160> g_diag_doc;
StaticJsonDocument<384> g_metrics_doc;
StaticJsonDocument<192> g_state_pub_doc;

// Pre-allocated serialization buffers
char g_deployment_output[224];
char g_diag_output[192];
char g_metrics_output[384];
char g_state_pub_output[192];

// Static buffers for stack reduction
StaticJsonDocument<512> g_identity_doc;
char g_identity_output[384];
StaticJsonDocument<384> g_error_ctx_doc;
char g_error_ctx_output[384];
StaticJsonDocument<256> g_rx_doc;
StaticJsonDocument<128> g_temp_state_doc;

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

void setupMQTTTopics() {
  snprintf(g_mqtt_topic_sub_unit,        sizeof(g_mqtt_topic_sub_unit),        "%s/%s/%s/%s",            g_control_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_state,       sizeof(g_mqtt_topic_pub_state),       "%s/%s/%s/%s/state",      g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_identity,    sizeof(g_mqtt_topic_pub_identity),    "%s/%s/%s/%s/identity",   g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_deployment,  sizeof(g_mqtt_topic_pub_deployment),  "%s/%s/%s/%s/deployment", g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_diagnostics, sizeof(g_mqtt_topic_pub_diagnostics), "%s/%s/%s/%s/diagnostics", g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_metrics,     sizeof(g_mqtt_topic_pub_metrics),     "%s/%s/%s/%s/metrics",    g_state_root, g_floor_id, g_room_id, g_unit_id);
  snprintf(g_mqtt_topic_pub_error,       sizeof(g_mqtt_topic_pub_error),       "%s/%s/%s/%s/error",      g_state_root, g_floor_id, g_room_id, g_unit_id);
}
