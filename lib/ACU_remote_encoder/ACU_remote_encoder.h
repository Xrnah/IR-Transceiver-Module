/*
 * ACU_remote_encoder.h
 * 
 * Defines the ACURemote class responsible for encoding and managing commands
 * for an Air Conditioning Unit (ACU) remote controller.
 * 
 * Features:
 * - Maintains the ACU’s current state, including fan speed, temperature, mode,
 *   louver position, and power state.
 * - Supports setting individual parameters or updating the entire ACU state.
 * - Encodes the ACU state into a 64-bit command format, including a 32-bit
 *   complement for error detection.
 * - Provides utility functions for converting the encoded command to a binary
 *   string (useful for debugging or display purposes).
 * - Supports JSON serialization and deserialization for easy integration with
 *   network or web APIs.
 * - Uses a signature string to customize encoding based on AC brand or protocol.
 * - Encapsulation of encoding logic ensures data integrity and prevents misuse.
 * 
 * Usage:
 * 1. Instantiate ACURemote with a signature string matching the target AC brand.
 * 2. Use setter methods or setState() to configure AC parameters.
 * 3. Call encodeCommand() to generate the latest encoded 64-bit command.
 * 4. Use toBinaryString() and toJSON() for debugging or data presentation.
 */


#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NTP.h>  // For getTimestamp()

// ====== Enumerations ======

// Operation modes for the AC unit.
enum class ACUMode {
  AUTO,
  COOL,
  HEAT,
  DRY,
  FAN,
  INVALID
};

enum class ACURemoteSignature : uint8_t {
  MitsubishiHeavy64,
  Unknown
};

// ====== Data Structures ======

// Struct representing the complete AC unit state.
typedef struct __attribute__((packed)) {
  uint8_t fan_speed;
  uint8_t temperature;
  ACUMode mode;
  uint8_t louver;
  bool power;
} ACUState;


// ====== Main Class: ACURemote ======

class ACURemote {
public:
  // Constructor: specify protocol signature (e.g., brand identifier)
  explicit ACURemote(ACURemoteSignature signature);
  explicit ACURemote(const char* signature_str);

  // === State Setters ===
  void setFanSpeed(uint8_t speed);
  void setTemperature(uint8_t temp);
  void setMode(ACUMode mode);
  void setLouver(uint8_t louver);
  void setPowerState(bool on);

  // Set full state at once
  void setState(uint8_t fan_speed, uint8_t temp, ACUMode mode, uint8_t louver, bool power = true);

  // === State Getters ===
  bool getPowerState() const;
  uint64_t getLastCommand() const;
  ACUState getState() const;

  // === Command Encoding ===
  // Encodes the current ACU state into a 64-bit command with 32-bit complement.
  uint64_t encodeCommand();

  // === Utilities ===
  // Fills a buffer with the binary string representation of a 64-bit value
  static void toBinaryString(uint64_t value, char* buf, size_t len, bool spaced = true);

  // Serialize current state into a JsonObject
  void toJSON(JsonObject doc) const;

  // Deserialize a JsonObject into internal state
  bool fromJSON(JsonObjectConst doc);

private:
  ACURemoteSignature signature_; // AC brand/protocol identifier
  ACUState state;             // Internal ACU state
  uint64_t last_command = 0;   // Most recently encoded command

  // === Encoding Helpers ===
  uint8_t encodeSignature() const;     // Brand-specific 4-bit identifier
  uint8_t encodeFanSpeed() const;      // Fan speed encoding
  uint8_t encodeTemperature() const;   // Temperature encoding
  uint8_t encodeMode() const;          // Encodes mode + power state
  uint8_t encodeLouver() const;        // Louver encoding

  // Converts enum mode to its string representation
  const char* modeToString(ACUMode mode) const;

  static ACURemoteSignature parseSignature(const char* signature_str);
};
