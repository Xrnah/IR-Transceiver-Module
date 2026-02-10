/*
 * ACU_ir_adapters.h
 *
 * Protocol adapters that map a generic ACUState to specific IRremoteESP8266
 * Mitsubishi Heavy protocol implementations.
 */

#pragma once

#include <Arduino.h>
#include <ir_MitsubishiHeavy.h>
#include "ACU_remote_encoder.h"

// Protocol adapter interface
class IACUAdapter {
public:
  virtual ~IACUAdapter() = default;
  virtual void begin() = 0;
  virtual bool send(const ACUState &state) = 0;
  virtual const char* name() const = 0;
};

// Mitsubishi Heavy 88-bit adapter
class MHI88Adapter : public IACUAdapter {
public:
  explicit MHI88Adapter(uint16_t pin = 4);
  void begin() override;
  bool send(const ACUState &state) override;
  const char* name() const override;

private:
  IRMitsubishiHeavy88Ac ir;
};

// Mitsubishi Heavy 152-bit adapter
class MHI152Adapter : public IACUAdapter {
public:
  explicit MHI152Adapter(uint16_t pin = 4);
  void begin() override;
  bool send(const ACUState &state) override;
  const char* name() const override;

private:
  IRMitsubishiHeavy152Ac ir;
};
