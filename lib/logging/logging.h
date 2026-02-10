#pragma once

#include <Arduino.h>
#include <cstdint>

#ifndef LOG_LEVEL
  #define LOG_LEVEL 2
#endif

#ifndef LOG_SERIAL_ENABLE
  #if LOG_LEVEL > 1
    #define LOG_SERIAL_ENABLE 1
  #else
    #define LOG_SERIAL_ENABLE 0
  #endif
#endif

#ifndef LOG_MQTT_ERROR_CONTEXT_MIN_LOG_LEVEL
  #ifdef LOG_MQTT_ERROR_CONTEXT_LEVEL
    #define LOG_MQTT_ERROR_CONTEXT_MIN_LOG_LEVEL LOG_MQTT_ERROR_CONTEXT_LEVEL
  #else
    #define LOG_MQTT_ERROR_CONTEXT_MIN_LOG_LEVEL 3
  #endif
#endif

enum class LogLevel : uint8_t {
  Error = 0,
  Warn = 1,
  Info = 2,
  Debug = 3,
};

/**
 * @brief Initialize logging after Serial.begin().
 *
 * LOG_SERIAL_ENABLE controls Serial initialization.
 * LOG_LEVEL controls local log verbosity.
 * LOG_MQTT_ERROR_CONTEXT_MIN_LOG_LEVEL controls MQTT /error publishing threshold.
 */
void initLogging();

void logError(const char* tag, const char* format, ...);
void logWarn(const char* tag, const char* format, ...);
void logInfo(const char* tag, const char* format, ...);
void logDebug(const char* tag, const char* format, ...);
