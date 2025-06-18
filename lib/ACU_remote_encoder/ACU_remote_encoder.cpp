#include "ACU_remote_encoder.h"

// Constructor: initialize with AC unit signature (e.g., brand/protocol type)
ACU_remote::ACU_remote(String signature)
  : signature(signature) {}

// ====== State Setters ======
// Individually set parts of the remote control state
void ACU_remote::setFanSpeed(uint8_t speed) {
  state.fanSpeed = speed;
}
void ACU_remote::setTemperature(uint8_t temp) {
  state.temperature = temp;
}
void ACU_remote::setMode(ACUMode mode) {
  state.mode = mode;
}
void ACU_remote::setLouver(uint8_t louver) {
  state.louver = louver;
}
void ACU_remote::setPowerState(bool on) {
  state.isOn = on;
}

// Set entire ACU state in one call
void ACU_remote::setState(uint8_t fanSpeed, uint8_t temp, ACUMode mode, uint8_t louver, bool isOn) {
  state.fanSpeed = fanSpeed;
  state.temperature = temp;
  state.mode = mode;
  state.louver = louver;
  state.isOn = isOn;
}

// ====== State Getters ======
bool ACU_remote::getPowerState() const {
  return state.isOn;
}

uint64_t ACU_remote::getLastCommand() const {
  return lastCommand;
}

ACUState ACU_remote::getState() const {
  return state;
}

// ====== Encode Command ======
// Encodes current state into a 64-bit command with a 32-bit complement
// Format: [command(32)] + [~command(32)]
uint64_t ACU_remote::encodeCommand() {
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

// ====== Utility: Convert 64-bit to Binary String ======
String ACU_remote::toBinaryString(uint64_t value, bool spaced) {
  String result = "";
  for (int i = 63; i >= 0; i--) {
    result += ((value >> i) & 1) ? '1' : '0';
    if (spaced && i % 4 == 0 && i != 0) result += ' ';
  }
  return result;
}

// ====== Serialize state to JSON string ======
String ACU_remote::toJSON() const {
  String json = "{";
  json += "\"fanSpeed\":" + String(state.fanSpeed) + ",";
  json += "\"temperature\":" + String(state.temperature) + ",";
  json += "\"mode\":\"" + modeToString(state.mode) + "\",";
  json += "\"louver\":" + String(state.louver) + ",";
  json += "\"isOn\":" + String(state.isOn ? "true" : "false");
  json += "}";
  return json;
}

// ====== Deserialize state from JSON string ======
// format: {"fanSpeed":2,"temperature":24,"mode":"cool","louver":3,"isOn":true}"
bool ACU_remote::fromJSON(const String& jsonString) {
  StaticJsonDocument<256> doc;  // ⚠️ Using deprecated type; see notes
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return false;
  }

  // Validate and extract all required fields
  if (!doc["fanSpeed"].is<uint8_t>() ||
      !doc["temperature"].is<uint8_t>() ||
      !doc["mode"].is<const char*>() ||
      !doc["louver"].is<uint8_t>() ||
      !doc["isOn"].is<bool>()) {
    return false;
  }

  // Extract and convert values
  uint8_t fanSpeed = doc["fanSpeed"];
  uint8_t temp = doc["temperature"];
  String modeStr = doc["mode"].as<String>();
  uint8_t louver = doc["louver"];
  bool isOn = doc["isOn"];

  // Convert mode string to enum
  ACUMode mode;
  if (modeStr == "auto") mode = ACUMode::AUTO;
  else if (modeStr == "cool") mode = ACUMode::COOL;
  else if (modeStr == "heat") mode = ACUMode::HEAT;
  else if (modeStr == "dry") mode = ACUMode::DRY;
  else if (modeStr == "fan") mode = ACUMode::FAN;
  else return false;  // Unrecognized mode

  setState(fanSpeed, temp, mode, louver, isOn);
  return true;
}

// ====== Private Helpers for Encoding ======

// Return protocol/brand-specific 4-bit identifier
uint8_t ACU_remote::encodeSignature() const {
  if (signature == "MITSUBISHI_HEAVY_64") return 0b0101;
  return 0b0000;  // Default fallback
}

// Fan speed encoded as 4 bits
uint8_t ACU_remote::encodeFanSpeed() const {
  switch (state.fanSpeed) {
    case 1: return 0b0010;
    case 2: return 0b1010;
    case 3: return 0b0110;
    case 4: return 0b1110;
    default: return 0b0000;
  }
}

// Temperature encoding based on internal reverse-engineering of protocol
uint8_t ACU_remote::encodeTemperature() const {
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
uint8_t ACU_remote::encodeMode() const {
  uint8_t base = 0;
  switch (state.mode) {
    case ACUMode::AUTO: base = 0b0001; break;
    case ACUMode::COOL: base = 0b0101; break;
    case ACUMode::HEAT: base = 0b0011; break;
    case ACUMode::DRY:  base = 0b1001; break;
    case ACUMode::FAN:  base = 0b1101; break;
    default: base = 0b0000; break;
  }

  if (!state.isOn) base &= ~0b0001;  // If off, clear LSB (power off)
  return base;
}

// Encode louver position as 4-bit value
uint8_t ACU_remote::encodeLouver() const {
  switch (state.louver) {
    case 1: return 0b0000;
    case 2: return 0b1000;
    case 3: return 0b0100;
    case 4: return 0b1100;
    default: return 0b0000;
  }
}

// Convert ACUMode enum to human-readable string
String ACU_remote::modeToString(ACUMode mode) const {
  switch (mode) {
    case ACUMode::AUTO: return "auto";
    case ACUMode::COOL: return "cool";
    case ACUMode::HEAT: return "heat";
    case ACUMode::DRY:  return "dry";
    case ACUMode::FAN:  return "fan";
    default: return "invalid";
  }
}
