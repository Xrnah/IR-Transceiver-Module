/*
 * ACU_remote_encoder.h
 * 
 * This header defines the ACU_remote class for encoding and managing commands
 * for an Air Conditioning Unit (ACU) remote controller.
 * 
 * Features:
 * - Stores the current ACU state, including fan speed, temperature, mode, louver position, and power state.
 * - Supports setting individual parameters or the full ACU state at once.
 * - Encodes the ACU state into a 64-bit command format, including a 32-bit complement for error detection.
 * - Provides a function to convert the 64-bit command to a binary string (for debugging or display).
 * - Converts the current ACU state into JSON format for easy integration with web or network APIs.
 * - Uses an ACU signature string to customize encoding (e.g., "mitsubishi-heavy").
 * - Encapsulates all encoding details privately to prevent misuse and ensure data integrity.
 * 
 * Usage:
 * - Instantiate ACU_remote with a signature string.
 * - Use setter methods or setState() to update ACU parameters.
 * - Call encodeCommand() to get the latest encoded 64-bit command.
 * - Use toBinaryString() and toJSON() for debugging or data presentation.
*/

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

enum class ACUMode {
  AUTO,
  COOL,
  HEAT,
  DRY,
  FAN,
  INVALID
};

struct ACUState {
  uint8_t fanSpeed = 0;
  uint8_t temperature = 0;
  ACUMode mode = ACUMode::AUTO;
  uint8_t louver = 0;
  bool isOn = true;
};

class ACU_remote {
public:
  ACU_remote(String signature);

  // Setters
  void setFanSpeed(uint8_t speed);
  void setTemperature(uint8_t temp);
  void setMode(ACUMode mode);
  void setLouver(uint8_t louver);
  void setPowerState(bool on);

  void setState(uint8_t fanSpeed, uint8_t temp, ACUMode mode, uint8_t louver, bool isOn = true);

  // Getters
  bool getPowerState() const;
  uint64_t getLastCommand() const;
  ACUState getState() const;

  // Encoding & command functions
  uint64_t encodeCommand();

  // Utility
  static String toBinaryString(uint64_t value, bool spaced = true);
  String toJSON() const;
  bool fromJSON(const String& jsonString);

private:
  String signature;
  ACUState state;
  uint64_t lastCommand = 0;

  // Encoding helpers
  uint8_t encodeSignature() const;
  uint8_t encodeFanSpeed() const;
  uint8_t encodeTemperature() const;
  uint8_t encodeMode() const;
  uint8_t encodeLouver() const;
  String modeToString(ACUMode mode) const;
};
