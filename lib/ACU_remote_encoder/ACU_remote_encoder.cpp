#include "ACU_remote_encoder.h"
#include "logging.h"

namespace {
constexpr const char* k_log_tag = "ACU";
} // namespace

// Constructor: initialize with AC unit signature (e.g., brand/protocol type)
ACURemote::ACURemote(String signature)
  : signature(signature) {}

// ====== State Setters ======
// Individually set parts of the remote control state
void ACURemote::setFanSpeed(uint8_t speed) {
  state.fan_speed = speed;
}
void ACURemote::setTemperature(uint8_t temp) {
  state.temperature = temp;
}
void ACURemote::setMode(ACUMode mode) {
  state.mode = mode;
}
void ACURemote::setLouver(uint8_t louver) {
  state.louver = louver;
}
void ACURemote::setPowerState(bool on) {
  state.power = on;
}

// Set entire ACU state in one call
void ACURemote::setState(uint8_t fan_speed, uint8_t temp, ACUMode mode, uint8_t louver, bool power) {
  state.fan_speed = fan_speed;
  state.temperature = temp;
  state.mode = mode;
  state.louver = louver;
  state.power = power;
}

// ====== State Getters ======
bool ACURemote::getPowerState() const {
  return state.power;
}

uint64_t ACURemote::getLastCommand() const {
  return lastCommand;
}

ACUState ACURemote::getState() const {
  return state;
}

// ====== Encode Command ======
// Encodes current state into a 64-bit command with a 32-bit complement
// Format: [command(32)] + [~command(32)]
uint64_t ACURemote::encodeCommand() {
  uint32_t command = 0;

  command |= ((uint32_t)encodeSignature() << 28);    // Signature (e.g., brand ID)
  command |= (0b0000 << 24);                         // Reserved bits
  command |= (0b0000 << 20);                         // Reserved bits
  command |= ((uint32_t)encodeFanSpeed() << 16);     // Fan speed
  command |= ((uint32_t)encodeTemperature() << 12);  // Temperature
  command |= ((uint32_t)encodeMode() << 8);          // Mode + power state
  command |= (0b0000 << 4);                          // Reserved bits
  command |= encodeLouver();                         // Louver setting

  uint32_t complement = ~command;                    // Calculate bitwise complement
  lastCommand = ((uint64_t)command << 32) | complement;
  return lastCommand;
}

// ====== Utility: Convert 64-bit to Binary String Buffer ======
void ACURemote::toBinaryString(uint64_t value, char* buf, size_t len, bool spaced) {
  if (!buf || len == 0) return;

  char* p = buf;
  for (int i = 63; i >= 0; i--) {
    if ((size_t)(p - buf) >= len - 1) break; // Ensure space for char + null terminator
    *p++ = ((value >> i) & 1) ? '1' : '0';

    if (spaced && i > 0 && i % 4 == 0) {
      if ((size_t)(p - buf) >= len - 1) break;
      *p++ = ' ';
    }
  }
  *p = '\0'; // Null-terminate the string
}

// ====== Serialize state to JsonObject ======
void ACURemote::toJSON(JsonObject doc) const {
  doc["fan_speed"] = state.fan_speed;
  doc["temperature"] = state.temperature;
  doc["mode"] = modeToString(state.mode);
  doc["louver"] = state.louver;
  doc["power"] = state.power;
  // Timestamp should be added by the caller (e.g., MQTT handler)
  // to ensure it's the timestamp of the event, not of state creation.
}

// ====== Deserialize state from JsonObject ======
// format: {"fan_speed":2,"temperature":24,"mode":"cool","louver":3,"power":true}"
bool ACURemote::fromJSON(JsonObjectConst doc) {
  // Validate and extract all required fields
  if (!doc["fan_speed"].is<uint8_t>() ||
      !doc["temperature"].is<uint8_t>() ||
      !doc["mode"].is<const char*>() ||
      !doc["louver"].is<uint8_t>() ||
      !doc["power"].is<bool>()) {
    logError(k_log_tag, "Invalid or missing fields in command.");
    return false;
  }

  // Extract and convert values
  uint8_t fan_speed = doc["fan_speed"];
  uint8_t temp = doc["temperature"];
  const char* modeStr = doc["mode"];
  uint8_t louver = doc["louver"];
  bool power = doc["power"];

  // Convert mode string to enum
  ACUMode mode;
  if (strcmp(modeStr, "auto") == 0) mode = ACUMode::AUTO;
  else if (strcmp(modeStr, "cool") == 0) mode = ACUMode::COOL;
  else if (strcmp(modeStr, "heat") == 0) mode = ACUMode::HEAT;
  else if (strcmp(modeStr, "dry") == 0) mode = ACUMode::DRY;
  else if (strcmp(modeStr, "fan") == 0) mode = ACUMode::FAN;
  else return false;  // Unrecognized mode

  setState(fan_speed, temp, mode, louver, power);
  return true;
}

// ====== Private Helpers for Encoding ======

// Return protocol/brand-specific 4-bit identifier
uint8_t ACURemote::encodeSignature() const {
  if (signature == "MITSUBISHI_HEAVY_64") return 0b0101;
  return 0b0000;  // Default fallback
}

// Fan speed encoded as 4 bits
uint8_t ACURemote::encodeFanSpeed() const {
  switch (state.fan_speed) {
    case 1: return 0b0000;
    case 2: return 0b1000;
    case 3: return 0b0100;

    case 4: return 0b0010; // Swing
    case 5: return 0b1010;
    case 6: return 0b0110;
    // case 4: return 0b1110; // Triple beeps
    default: return 0b0000;
  }
}

// Temperature encoding based on internal reverse-engineering of protocol
uint8_t ACURemote::encodeTemperature() const {
  switch (state.temperature) {
    case 18: return 0b0100;
    case 19: return 0b1100;
    case 20: return 0b0010;
    case 21: return 0b1010;
    case 22: return 0b0110;
    case 23: return 0b1110;
    case 24: return 0b0001;
    case 25: return 0b1001;
    case 26: return 0b0101;
    case 27: return 0b1101;
    case 28: return 0b0011;
    case 29: return 0b1011;
    case 30: return 0b0111;
    default: return 0b0000;
  }
}

// Encode mode and power state (LSB represents power)
uint8_t ACURemote::encodeMode() const {
  uint8_t base = 0;
  switch (state.mode) {
    case ACUMode::AUTO: base = 0b0001; break;
    case ACUMode::COOL: base = 0b0101; break;
    case ACUMode::HEAT: base = 0b0011; break;
    case ACUMode::DRY:  base = 0b1001; break;
    case ACUMode::FAN:  base = 0b1101; break;
    default: base = 0b0000; break;
  }

  if (!state.power) base &= ~0b0001;  // If off, clear LSB (power off)
  return base;
}

// Encode louver position as 4-bit value
uint8_t ACURemote::encodeLouver() const {
  switch (state.louver) {
    case 0: return 0b0010; // 0 deg
    case 1: return 0b1010; // 22.5 deg
    case 2: return 0b0110; // 45 deg
    case 3: return 0b1110; // 67.5 deg
    case 4: return 0b0000; // swing
    // case 3: return 0b0011; // Triple beeps - not encoded
    // case 7: return 0b0111; // Triple beeps - not encoded
    // case 11: return 0b1011; // Triple beeps
    // case 15: return 0b1111; // Triple beeps
    default: return 0b0010; // 0 deg default
  }
}

// Convert ACUMode enum to human-readable string
const char* ACURemote::modeToString(ACUMode mode) const {
  switch (mode) {
    case ACUMode::AUTO: return "auto";
    case ACUMode::COOL: return "cool";
    case ACUMode::HEAT: return "heat";
    case ACUMode::DRY:  return "dry";
    case ACUMode::FAN:  return "fan";
    default: return "invalid";
  }
}
