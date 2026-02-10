/*
 ************************************************************************************
 *                                                                                  *
 *    ** IMPORTANT: DO NOT commit your secrets! **                                  *
 *                                                                                  *
 *    To use this, copy this file to 'wifi_credentials.h' and fill in your actual   *
 *    credentials. The 'wifi_credentials.h' file is ignored by Git.                 *
 *                                                                                  *
 ************************************************************************************
 */

/*
 * wifi_credentials.cpp (Template)
 *
 * This file contains the WiFi credentials for the project.
 * It is included by WiFiManager.cpp and should be added to .gitignore
 * to prevent sensitive information from being committed to version control.
 */

#include "WiFiData.h"

namespace CustomWiFi {

// --- Shared Passwords ---
const char* constantPASS1 = "PASS1";

const WiFiCredential k_wifi_table[] = {
    {"WiFI", constantPASS1},
    {"through", "anotherPass"},
    {"Example", "ThisIsThePassword"},
    // Add more networks here
};

const int k_wifi_count = sizeof(k_wifi_table) / sizeof(k_wifi_table[0]);

} // namespace CustomWiFi
