#include "ACU_ir_adapters.h"

static uint8_t mapModeToMhi(ACUMode mode) {
  switch (mode) {
    case ACUMode::COOL: return kMitsubishiHeavyCool;
    case ACUMode::HEAT: return kMitsubishiHeavyHeat;
    case ACUMode::DRY:  return kMitsubishiHeavyDry;
    case ACUMode::FAN:  return kMitsubishiHeavyFan;
    case ACUMode::AUTO:
    default:            return kMitsubishiHeavyAuto;
  }
}

static uint8_t mapFan88(uint8_t fan) {
  switch (fan) {
    case 2: return kMitsubishiHeavy88FanLow;
    case 3: return kMitsubishiHeavy88FanMed;
    case 4: return kMitsubishiHeavy88FanHigh;
    case 5: return kMitsubishiHeavy88FanTurbo;
    case 6: return kMitsubishiHeavy88FanEcono;
    case 1:
    case 0:
    default: return kMitsubishiHeavy88FanAuto;
  }
}

static uint8_t mapFan152(uint8_t fan) {
  switch (fan) {
    case 2: return kMitsubishiHeavy152FanLow;
    case 3: return kMitsubishiHeavy152FanMed;
    case 4: return kMitsubishiHeavy152FanHigh;
    case 5: return kMitsubishiHeavy152FanMax;
    case 6: return kMitsubishiHeavy152FanTurbo;
    case 1:
    case 0:
    default: return kMitsubishiHeavy152FanAuto;
  }
}

static uint8_t mapSwingV88(uint8_t louver) {
  switch (louver) {
    case 0: return kMitsubishiHeavy88SwingVHighest;
    case 1: return kMitsubishiHeavy88SwingVHigh;
    case 2: return kMitsubishiHeavy88SwingVMiddle;
    case 3: return kMitsubishiHeavy88SwingVLow;
    case 4: return kMitsubishiHeavy88SwingVAuto;
    default: return kMitsubishiHeavy88SwingVAuto;
  }
}

static uint8_t mapSwingV152(uint8_t louver) {
  switch (louver) {
    case 0: return kMitsubishiHeavy152SwingVHighest;
    case 1: return kMitsubishiHeavy152SwingVHigh;
    case 2: return kMitsubishiHeavy152SwingVMiddle;
    case 3: return kMitsubishiHeavy152SwingVLow;
    case 4: return kMitsubishiHeavy152SwingVAuto;
    default: return kMitsubishiHeavy152SwingVAuto;
  }
}

Mhi88Adapter::Mhi88Adapter(uint16_t pin)
  : ir(pin) {}

void Mhi88Adapter::begin() {
  ir.begin();
}

bool Mhi88Adapter::send(const ACUState &state) {
  ir.stateReset();
  ir.setPower(state.power);
  ir.setMode(mapModeToMhi(state.mode));
  ir.setTemp(state.temperature);
  ir.setFan(mapFan88(state.fan_speed));
  ir.setSwingVertical(mapSwingV88(state.louver));
  ir.setSwingHorizontal(kMitsubishiHeavy88SwingHOff);
  ir.send();
  return true;
}

const char* Mhi88Adapter::name() const {
  return "MHI-88";
}

Mhi152Adapter::Mhi152Adapter(uint16_t pin)
  : ir(pin) {}

void Mhi152Adapter::begin() {
  ir.begin();
}

bool Mhi152Adapter::send(const ACUState &state) {
  ir.stateReset();
  ir.setPower(state.power);
  ir.setMode(mapModeToMhi(state.mode));
  ir.setTemp(state.temperature);
  ir.setFan(mapFan152(state.fan_speed));
  ir.setSwingVertical(mapSwingV152(state.louver));
  ir.setSwingHorizontal(kMitsubishiHeavy152SwingHOff);
  ir.send();
  return true;
}

const char* Mhi152Adapter::name() const {
  return "MHI-152";
}
