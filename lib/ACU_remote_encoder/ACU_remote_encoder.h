/*
 * ACU_remote_encoder.h
 * 
 * Defines the ACU_remote class responsible for encoding and managing commands
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
 * 1. Instantiate ACU_remote with a signature string matching the target AC brand.
 * 2. Use setter methods or setState() to configure AC parameters.
 * 3. Call encodeCommand() to generate the latest encoded 64-bit command.
 * 4. Use toBinaryString() and toJSON() for debugging or data presentation.
 */


#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

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

// ====== Data Structures ======

// Struct representing the complete AC unit state.
struct ACUState {
  uint8_t fanSpeed = 0;          // Fan speed (1–4 typically)
  uint8_t temperature = 0;       // Temperature in °C (usually 18–30)
  ACUMode mode = ACUMode::AUTO;  // Operating mode
  uint8_t louver = 0;            // Louver position (e.g., 1–4)
  bool isOn = true;              // Power state
};

// ====== Main Class: ACU_remote ======

class ACU_remote {
public:
  // Constructor: specify protocol signature (e.g., brand identifier)
  ACU_remote(String signature);

  // === State Setters ===
  void setFanSpeed(uint8_t speed);
  void setTemperature(uint8_t temp);
  void setMode(ACUMode mode);
  void setLouver(uint8_t louver);
  void setPowerState(bool on);

  // Set full state at once
  void setState(uint8_t fanSpeed, uint8_t temp, ACUMode mode, uint8_t louver, bool isOn = true);

  // === State Getters ===
  bool getPowerState() const;
  uint64_t getLastCommand() const;
  ACUState getState() const;

  // === Command Encoding ===
  // Encodes the current ACU state into a 64-bit command with 32-bit complement.
  uint64_t encodeCommand();

  // === Utilities ===
  // Convert 64-bit command to binary string, optionally spaced every 4 bits
  static String toBinaryString(uint64_t value, bool spaced = true);

  // Serialize current state to JSON string
  String toJSON() const;

  // Deserialize JSON string into internal state
  bool fromJSON(const String& jsonString);

private:
  String signature;           // AC brand/protocol identifier
  ACUState state;             // Internal ACU state
  uint64_t lastCommand = 0;   // Most recently encoded command

  // === Encoding Helpers ===
  uint8_t encodeSignature() const;     // Brand-specific 4-bit identifier
  uint8_t encodeFanSpeed() const;      // Fan speed encoding
  uint8_t encodeTemperature() const;   // Temperature encoding
  uint8_t encodeMode() const;          // Encodes mode + power state
  uint8_t encodeLouver() const;        // Louver encoding

  // Converts enum mode to string (for JSON)
  String modeToString(ACUMode mode) const;
};
