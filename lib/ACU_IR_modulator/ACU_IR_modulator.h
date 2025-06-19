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
 * - Provides constants/macros for easy access to IR signal timing parameters.
 * - Declares external objects and buffers for IR transmission.
 * - Declares functions to convert binary commands into IR duration sequences.
 * - Includes legacy support for parsing from binary strings (useful for debugging).
 * - Includes a debug utility function to read 64-bit binary input from Serial
 *   and send the corresponding IR signal.
 * 
 * Usage:
 * - Set 'selectedProtocol' to the desired IRProtocolConfig (e.g., MITSUBISHI_HEAVY_64).
 * - Use parseBinaryToDurations() to convert commands to IR timing sequences.
 * - Use debugIRInput() to test IR sending interactively via Serial input.
 * 
 * Target platform: ESP8266 with IR LED on pin 4 (default)
 */

#pragma once

#include <Arduino.h>
#include <IRsend.h>

struct IRProtocolConfig {
  const uint16_t hdrMark;
  const uint16_t hdrSpace;
  const uint16_t bitMark;
  const uint16_t oneSpace;
  const uint16_t zeroSpace;
};

// ACU-specific timing config
constexpr IRProtocolConfig MITSUBISHI_HEAVY_64 = {
  6000,   // hdrMark
  7300,   // hdrSpace
  500,    // bitMark
  3300,   // oneSpace
  1400    // zeroSpace
};

// Example default protocol (optional)
constexpr IRProtocolConfig DEFAULT_PROTOCOL = {
  5000,  // hdrMark (placeholder)
  5000,  // hdrSpace
  400,   // bitMark
  2000,  // oneSpace
  1000   // zeroSpace
};

// Pointer to the active protocol configuration
extern const IRProtocolConfig* selectedProtocol;

// Constants for IR signal parameters (access via selectedProtocol)
#define kHdrMark   (selectedProtocol->hdrMark)
#define kHdrSpace  (selectedProtocol->hdrSpace)
#define kBitMark   (selectedProtocol->bitMark)
#define kOneSpace  (selectedProtocol->oneSpace)
#define kZeroSpace (selectedProtocol->zeroSpace)

constexpr uint8_t rawDataLength = 133;
constexpr uint16_t kIrLedPin = 4;  // default ESP8266 IR LED pin

extern IRsend irsend;                     // Extern declaration
extern uint16_t durations[rawDataLength]; // Durations buffer

// Main parser for internal 64-bit command
void parseBinaryToDurations(uint64_t binaryInput, uint16_t *durations, size_t &len);

// Optional legacy parser for Serial debug input
void parseBinaryToDurations(const String &binaryInput, uint16_t *durations, size_t &len);

// Serial debugging for binary input
void debugIRInput();
