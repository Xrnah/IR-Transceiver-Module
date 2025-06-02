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
