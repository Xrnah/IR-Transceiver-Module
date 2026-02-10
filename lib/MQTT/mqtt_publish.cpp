#include "mqtt_internal.h"

#if !defined(ARDUINO_ARCH_ESP8266)
#error "ESP8266 only"
#endif

namespace {
void formatIpAddress(char* buf, size_t len, const IPAddress& ip) {
  if (buf == nullptr || len == 0) return;
  snprintf(buf, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}
} // namespace

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
    logError(k_log_tag, "Publish failed (topic=%s len=%u).", g_mqtt_topic_pub_state, (unsigned int)len);
    publishMQTTErrorContext("publish_failed", g_mqtt_topic_pub_state, (const uint8_t*)g_state_pub_output, len, 0);
    g_mqtt_publish_failures++;
  }
}

void publishIdentity() {
  if (!g_mqtt_client.connected()) return;
  g_identity_doc.clear();

  char client_id_str[32];
  snprintf(client_id_str, sizeof(client_id_str), "ESP8266Client-%06X", ESP.getChipId());

  g_identity_doc["device_id"] = client_id_str;
  g_identity_doc["mac_address"] = WiFi.macAddress();
  g_identity_doc["acu_remote_model"] = ACU_REMOTE_MODEL; 
  // g_identity_doc["room_type"] = DEFINED_ROOM_TYPE;
  g_identity_doc["room_type_id"] = DEFINED_ROOM_TYPE_ID;
  g_identity_doc["department"] = DEFINED_DEPARTMENT;

  if (g_identity_doc.overflowed()) logWarn(k_log_tag, "Identity JSON doc overflow");
  size_t n = serializeJson(g_identity_doc, g_identity_output, sizeof(g_identity_output));
  if (n >= sizeof(g_identity_output)) logWarn(k_log_tag, "Identity output truncated");

  g_mqtt_client.publish(g_mqtt_topic_pub_identity, g_identity_output, true); // Retain identity info
  g_identity_doc.clear();
}

void publishDeployment() {
  if (!g_mqtt_client.connected()) return;
  g_deployment_doc.clear();

  char ip_buffer[16];
  formatIpAddress(ip_buffer, sizeof(ip_buffer), WiFi.localIP());
  g_deployment_doc["ip_address"] = ip_buffer;
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

void publishMQTTErrorContext(const char* error, const char* topic, const uint8_t* payload, unsigned int length, int rc) {
  const char* error_str = (error != nullptr) ? error : "unknown_error";
  const char* topic_str = (topic != nullptr) ? topic : "n/a";
  logError(k_log_tag, "Error context: %s (topic=%s rc=%d len=%u)", error_str, topic_str, rc, length);

#if LOG_LEVEL >= LOG_MQTT_ERROR_CONTEXT_MIN_LOG_LEVEL
  ErrorContextSnapshot snapshot;
  snapshot.has_data = true;
  strncpy(snapshot.error, error_str, sizeof(snapshot.error) - 1);
  if (topic != nullptr) {
    strncpy(snapshot.topic, topic, sizeof(snapshot.topic) - 1);
  } else {
    strncpy(snapshot.topic, "n/a", sizeof(snapshot.topic) - 1);
  }
  snapshot.rc = rc;

  if (payload != nullptr && length > 0) {
    size_t copy_len = (length < k_error_payload_max) ? length : k_error_payload_max;
    memcpy(snapshot.payload, payload, copy_len);
    snapshot.payload[copy_len] = '\0';
    snapshot.payload_len = length;
    snapshot.has_payload = true;
  }

  if (!g_mqtt_client.connected()) {
    queueErrorContextSnapshot(snapshot);
    return;
  }

  if (!publishErrorContextSnapshot(snapshot)) {
    g_mqtt_publish_failures++;
    queueErrorContextSnapshot(snapshot);
  }
#else
  (void)payload;
  (void)length;
  (void)rc;
#endif
}

bool publishErrorContextSnapshot(const ErrorContextSnapshot& snapshot) {
  if (!snapshot.has_data) return true;

  g_error_ctx_doc.clear();

  char time_buffer[30];
  getTimestamp(time_buffer, sizeof(time_buffer));
  g_error_ctx_doc["ts"] = time_buffer;
  g_error_ctx_doc["error"] = snapshot.error;
  g_error_ctx_doc["broker"] = g_mqtt_server;
  g_error_ctx_doc["port"] = g_mqtt_port;
  if (snapshot.topic[0] != '\0' && strcmp(snapshot.topic, "n/a") != 0) {
    g_error_ctx_doc["topic"] = snapshot.topic;
  }
  if (snapshot.rc != 0) g_error_ctx_doc["rc"] = snapshot.rc;

  if (snapshot.has_payload) {
    g_error_ctx_doc["payload_len"] = snapshot.payload_len;
    g_error_ctx_doc["payload"] = snapshot.payload;
  }

  size_t n = serializeJson(g_error_ctx_doc, g_error_ctx_output, sizeof(g_error_ctx_output));
  bool is_ok = g_mqtt_client.publish(
    g_mqtt_topic_pub_error,
    (const uint8_t*)g_error_ctx_output,
    n,
    false
  );

  g_error_ctx_doc.clear();
  return is_ok;
}

void queueErrorContextSnapshot(const ErrorContextSnapshot& snapshot) {
  g_last_error_ctx = snapshot;
  g_has_queued_error_ctx = true;
}

void publishQueuedErrorContextIfAny() {
#if LOG_LEVEL >= LOG_MQTT_ERROR_CONTEXT_MIN_LOG_LEVEL
  if (!g_has_queued_error_ctx) return;
  if (!g_mqtt_client.connected()) return;
  if (publishErrorContextSnapshot(g_last_error_ctx)) {
    g_has_queued_error_ctx = false;
  } else {
    g_mqtt_publish_failures++;
  }
#endif
}

void publishOnReconnect() {
  publishIdentity();
  publishDeployment();
  publishDiagnostics();
  publishMetrics();
  publishQueuedErrorContextIfAny();

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
