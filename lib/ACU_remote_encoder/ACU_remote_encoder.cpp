#include "ACU_remote_encoder.h"

ACU_remote::ACU_remote(String signature)
  : signature(signature) {}

// Setters
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

void ACU_remote::setState(uint8_t fanSpeed, uint8_t temp, ACUMode mode, uint8_t louver, bool isOn) {
  state.fanSpeed = fanSpeed;
  state.temperature = temp;
  state.mode = mode;
  state.louver = louver;
  state.isOn = isOn;
}

// Getters
bool ACU_remote::getPowerState() const {
  return state.isOn;
}
uint64_t ACU_remote::getLastCommand() const {
  return lastCommand;
}
ACUState ACU_remote::getState() const {
  return state;
}

// Encode command into 64-bit with complement
uint64_t ACU_remote::encodeCommand() {
  uint32_t command = 0;
  command |= (encodeSignature() << 28);
  command |= (0b0000 << 24);  // Reserved
  command |= (0b0000 << 20);  // Reserved
  command |= ((uint32_t)encodeFanSpeed() << 16);
  command |= ((uint32_t)encodeTemperature() << 12);
  command |= ((uint32_t)encodeMode() << 8);
  command |= (0b0000 << 4);  // Reserved
  command |= encodeLouver();

  uint32_t complement = ~command;
  lastCommand = ((uint64_t)command << 32) | complement;
  return lastCommand;
}

// Static utility for binary string conversion
String ACU_remote::toBinaryString(uint64_t value, bool spaced) {
  String result = "";
  for (int i = 63; i >= 0; i--) {
    result += ((value >> i) & 1) ? '1' : '0';
    if (spaced && i % 4 == 0 && i != 0) result += ' ';
  }
  return result;
}

// JSON output of current state
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

// Private encode helpers
uint8_t ACU_remote::encodeSignature() const {
  if (signature == "mitsubishi-heavy") return 0b0101;
  // Add more signatures if the brand follows the same 
  //  binary structure.
  return 0b0000;
}

uint8_t ACU_remote::encodeFanSpeed() const {
  switch (state.fanSpeed) {
    case 1: return 0b0010;
    case 2: return 0b1010;
    case 3: return 0b0110;
    case 4: return 0b1110;
    default: return 0b0000;
  }
}

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

uint8_t ACU_remote::encodeMode() const {
  uint8_t base = 0;
  switch (state.mode) {
    case ACUMode::AUTO: base = 0b0001; break;
    case ACUMode::COOL: base = 0b0101; break;
    case ACUMode::HEAT: base = 0b0011; break;
    case ACUMode::DRY: base = 0b1001; break;
    case ACUMode::FAN: base = 0b1101; break;
    default: base = 0b0000; break;
  }

  if (!state.isOn) base &= ~0b0001;  // Clear LSB if off
  return base;
}

uint8_t ACU_remote::encodeLouver() const {
  switch (state.louver) {
    case 1: return 0b0000;
    case 2: return 0b1000;
    case 3: return 0b0100;
    case 4: return 0b1100;
    default: return 0b0000;
  }
}

String ACU_remote::modeToString(ACUMode mode) const {
  switch (mode) {
    case ACUMode::AUTO: return "auto";
    case ACUMode::COOL: return "cool";
    case ACUMode::HEAT: return "heat";
    case ACUMode::DRY: return "dry";
    case ACUMode::FAN: return "fan";
    default: return "invalid";
  }
}
