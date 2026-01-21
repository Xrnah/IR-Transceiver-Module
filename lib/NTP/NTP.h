#pragma once
#include <Arduino.h>
#include <time.h>

extern const char* NTP_ADDR1_P;
extern const char* NTP_ADDR2_P;

/**
 * @brief Initializes NTP time synchronization for the microcontroller.
 * Uses Philippine Time (UTC+8).
 */
void setupTime();

/**
 * @brief Fills a buffer with the current local time as a formatted string.
 * Format: YYYY-MM-DD HH:MM:SS (GMT+8)
 * 
 * @param buffer The character buffer to write the timestamp into.
 * @param len The size of the buffer.
 */
void getTimestamp(char* buffer, size_t len);
