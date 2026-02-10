/*
 * ACU_IR_modulator.h
 * 
 * This header defines IR protocol configurations and interfaces for encoding
 * 64-bit air conditioner remote commands into IR signal durations.
 * 
 * Features:
 * - Defines timing parameters (mark and space durations) for ACU IR protocols
 *   such as Mitsubishi Heavy 64-bit protocol.
 * - Supports selection of active IR protocol via a pointer for flexibility.
 * - Declares external objects and buffers for IR transmission.
 * - Declares functions to convert binary commands into IR duration sequences.
 * - Includes legacy support for parsing from binary strings (useful for debugging).
 * - Includes a debug utility function to read 64-bit binary input from Serial
 *   and send the corresponding IR signal.
 * 
 * Usage:
 * - Set 'g_selected_protocol' to the desired IRProtocolConfig (e.g., k_mitsubishi_heavy_64).
 * - Use parseBinaryToDurations() to convert commands to IR timing sequences.
 * - Use debugIRInput() to test IR sending interactively via Serial input.
 * 
 * Target platform: ESP8266 with IR LED on pin 4 (default)
 */

#pragma once

#include <Arduino.h>
#include <IRsend.h>

struct IRProtocolConfig {
  const uint16_t hdr_mark;
  const uint16_t hdr_space;
  const uint16_t bit_mark;
  const uint16_t one_space;
  const uint16_t zero_space;
};

// ACU-specific timing config
constexpr IRProtocolConfig k_mitsubishi_heavy_64 = {
  6000,   // hdr_mark
  7300,   // hdr_space
  500,    // bit_mark
  3300,   // one_space
  1400    // zero_space
};

// Example default protocol (optional)
constexpr IRProtocolConfig k_default_protocol = {
  5000,  // hdr_mark (placeholder)
  5000,  // hdr_space
  400,   // bit_mark
  2000,  // one_space
  1000   // zero_space
};

// Pointer to the active protocol configuration
extern const IRProtocolConfig* g_selected_protocol;

constexpr uint8_t raw_data_length = 133;
constexpr uint16_t ir_led_pin = 4;  // default ESP8266 IR LED pin

extern IRsend g_ir_send;                         // Extern declaration
extern uint16_t g_durations[raw_data_length];    // Durations buffer

// Main parser for internal 64-bit command
bool parseBinaryToDurations(uint64_t binary_input, uint16_t *durations, size_t &len);

// Optional legacy parser for Serial debug input
bool parseBinaryToDurations(const String &binary_input, uint16_t *durations, size_t &len);

// Serial debugging for binary input
void debugIRInput();
