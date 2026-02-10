#pragma once

/*
 * NTP.h
 *
 * NTP time synchronization helpers for ESP8266.
 */

#include <Arduino.h>
#include <time.h>

/**
 * @brief Initialize NTP time synchronization (UTC+8).
 */
void setupTime();

/**
 * @brief Get the current local time as a formatted string.
 *
 * @param buffer Destination buffer.
 * @param len Buffer size.
 */
void getTimestamp(char* buffer, size_t len);
