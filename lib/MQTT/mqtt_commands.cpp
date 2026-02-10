#include "mqtt_internal.h"

#if !defined(ARDUINO_ARCH_ESP8266)
#error "ESP8266 only"
#endif

bool isTopicMatchingModule(char* topic) {
  return strcmp(topic, g_mqtt_topic_sub_unit) == 0;
}

void handleReceivedCommand(char* topic, byte* payload, unsigned int length) {
  unsigned long rx_time_ms = millis();

  // Deserialize incoming JSON
  g_rx_doc.clear();
  DeserializationError err = deserializeJson(g_rx_doc, payload, length);
  if (err) {
    logError(k_log_tag, "JSON parse failed: %s (topic=%s len=%u)", err.c_str(), topic, length);
    publishMQTTErrorContext("json_parse_failed", topic, payload, length, 0);
    g_commands_failed_parse++;
    return;
  }

  g_commands_received_counter++;

  // Handle potential nested "state" object
  JsonObjectConst state_obj = g_rx_doc.containsKey("state") ? g_rx_doc["state"] : g_rx_doc.as<JsonObjectConst>();

  if (!g_acu_remote.fromJSON(state_obj)) {
    logError(k_log_tag, "Invalid command structure (topic=%s len=%u).", topic, length);
    publishMQTTErrorContext("invalid_command_structure", topic, payload, length, 0);
    g_commands_failed_struct++;
    return;
  }

#if USE_ACU_ADAPTER
  logDebug(k_log_tag, "JSON parsed. Adapter: %s", g_acu_adapter.name());
#else
  logDebug(k_log_tag, "JSON parsed. Using MHI_64 IR modulator.");
#endif

  yield(); // Allow ESP8266 background tasks

#if USE_ACU_ADAPTER
  // Send IR using adapter
  if (g_acu_adapter.send(g_acu_remote.getState())) {
    g_commands_executed_counter++;
  } else {
    logError(k_log_tag, "Failed to send IR command (topic=%s len=%u).", topic, length);
    publishMQTTErrorContext("ir_send_failed", topic, payload, length, 0);
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
    logError(k_log_tag, "Failed to parse command for IR sending (topic=%s len=%u).", topic, length);
    publishMQTTErrorContext("ir_parse_failed", topic, payload, length, 0);
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
    g_temp_state_doc.clear();
    g_acu_remote.toJSON(g_temp_state_doc.to<JsonObject>());
    publishACUState(g_temp_state_doc.as<JsonObject>());

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
