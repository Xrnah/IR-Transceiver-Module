/*
 * WiFiData.h
 *
 * This file defines the data structures and enumerations used by the
 * CustomWiFi::WiFiManager library. By separating these definitions, we avoid
 * circular dependencies between header files.
 */

#pragma once

#include <stdint.h> // For uint32_t

namespace CustomWiFi {
  struct WiFiCredential {
    const char* ssid;
    const char* password;
  };

  // Declare the array of credentials as extern
  extern const WiFiCredential wifiTable[];

  // Declare the number of credentials
  extern const int WIFI_COUNT;

} // namespace CustomWiFi