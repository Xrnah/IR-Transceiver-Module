#pragma once

#include <Arduino.h>
#include <cstdint>

#ifndef LOG_LEVEL
  #define LOG_LEVEL 2
#endif

#ifndef LOG_SERIAL_ENABLE
  #define LOG_SERIAL_ENABLE 1
#endif

enum class LogLevel : uint8_t {
  Error = 0,
  Warn = 1,
  Info = 2,
  Debug = 3,
};

/**
 * @brief Initialize logging after Serial.begin().
 */
void initLogging();

void logError(const char* tag, const char* format, ...);
void logWarn(const char* tag, const char* format, ...);
void logInfo(const char* tag, const char* format, ...);
void logDebug(const char* tag, const char* format, ...);
