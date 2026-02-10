#include "NTP.h"
#include "logging.h"
#include "secrets.h" // Include secrets for NTP server addresses

namespace {
constexpr const char* k_log_tag = "NTP";
constexpr long utc_offset_seconds = 8 * 3600;
const char* g_ntp_addr1 = NTP_SERVER_1;
const char* g_ntp_addr2 = NTP_SERVER_2;
} // namespace

void setupTime() {
  configTime(utc_offset_seconds, 0, g_ntp_addr1, g_ntp_addr2); // UTC+8

  logInfo(k_log_tag, "Waiting for NTP time sync");
  time_t now = time(nullptr);
  while (now < utc_offset_seconds * 2) {
    delay(500);
    now = time(nullptr);
  }
  logInfo(k_log_tag, "Time synchronized.");
}

void getTimestamp(char* buffer, size_t len) {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  strftime(buffer, len, "%Y-%m-%d %H:%M:%S", timeinfo);
}
