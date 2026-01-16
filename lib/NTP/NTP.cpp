#include "NTP.h"
#include "secrets.h" // Include secrets for NTP server addresses

const char* NTP_ADDR1 = NTP_SERVER_1;
const char* NTP_ADDR2 = NTP_SERVER_2;

void setupTime() {
  configTime(8 * 3600, 0, NTP_ADDR1, NTP_ADDR2); // UTC+8

  Serial.print("[NTP] Waiting for NTP time sync");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\n[NTP] Time synchronized.");
}

String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S (GMT+8)", timeinfo);
  return String(buffer);
}
