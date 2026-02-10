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
  extern const WiFiCredential k_wifi_table[];

  // Declare the number of credentials
  extern const int k_wifi_count;

} // namespace CustomWiFi
