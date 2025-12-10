#pragma once
#include <Arduino.h>
#include <time.h>

#define NTP_ADDR1 "pool.ntp.org"
#define NTP_ADDR2 "time.google.com"

/**
 * @brief Initializes NTP time synchronization for the microcontroller.
 * Uses Philippine Time (UTC+8).
 */
void setupTime();

/**
 * @brief Returns current local time as a formatted string.
 * Format: YYYY-MM-DD HH:MM:SS
 * 
 * @return String formatted timestamp
 */
String getTimestamp();
