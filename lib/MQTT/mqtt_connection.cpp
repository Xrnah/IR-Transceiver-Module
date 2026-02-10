#include "mqtt_internal.h"

#if !defined(ARDUINO_ARCH_ESP8266)
#error "ESP8266 only"
#endif

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
      logError(k_log_tag, "Connect failed (rc=%d broker=%s port=%d), retrying...", rc, g_mqtt_server, g_mqtt_port);
      publishMQTTErrorContext("connect_failed", nullptr, nullptr, 0, rc);
    }
  }
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
