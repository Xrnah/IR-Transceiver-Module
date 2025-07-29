#pragma once
#include <Arduino.h>
#include <time.h>

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
