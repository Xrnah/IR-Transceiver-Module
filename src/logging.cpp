#include "logging.h"

#include <cstdarg>
#include <cstdio>

namespace {

constexpr size_t k_max_log_length = 256;
bool g_is_logging_ready = false;

const char* levelToString(LogLevel level) {
  switch (level) {
    case LogLevel::Error: return "ERROR";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Debug: return "DEBUG";
  }
  return "INFO";
}

bool isLogEnabled(LogLevel level) {
  return static_cast<uint8_t>(level) <= LOG_LEVEL;
}

void logMessage(LogLevel level, const char* tag, const char* format, va_list args) {
  if (!isLogEnabled(level)) return;
  if (!g_is_logging_ready) return;

  char message[k_max_log_length];
  vsnprintf(message, sizeof(message), format, args);

  const char* level_str = levelToString(level);
  const char* tag_str = (tag != nullptr) ? tag : "GEN";

  Serial.printf("[%s] [%s] %s\n", level_str, tag_str, message);
}

} // namespace

void initLogging() {
  g_is_logging_ready = true;
}

void logError(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(LogLevel::Error, tag, format, args);
  va_end(args);
}

void logWarn(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(LogLevel::Warn, tag, format, args);
  va_end(args);
}

void logInfo(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(LogLevel::Info, tag, format, args);
  va_end(args);
}

void logDebug(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(LogLevel::Debug, tag, format, args);
  va_end(args);
}
