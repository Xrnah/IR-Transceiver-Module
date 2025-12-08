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
 * wifi_credentials.h (Template)
 *
 * This file contains the WiFi credentials for the project.
 * It is included by WiFiManager.cpp and should be added to .gitignore
 * to prevent sensitive information from being committed to version control.
 */

#pragma once

#include "WiFiManager.h"

// --- Known WiFi Networks Table ---
WiFiCredential wifiTable[] = {

    {"Sample_SSID1", "Plaintext_Pass1"},
    {"Sample_SSID2", "Plaintext_Pass2"},
    {"Sample_SSID3", "Plaintext_Pass3"},

};