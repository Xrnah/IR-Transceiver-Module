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
class IAcuAdapter {
public:
  virtual ~IAcuAdapter() = default;
  virtual void begin() = 0;
  virtual bool send(const ACUState &state) = 0;
  virtual const char* name() const = 0;
};

// Mitsubishi Heavy 88-bit adapter
class Mhi88Adapter : public IAcuAdapter {
public:
  explicit Mhi88Adapter(uint16_t pin = 4);
  void begin() override;
  bool send(const ACUState &state) override;
  const char* name() const override;

private:
  IRMitsubishiHeavy88Ac ir;
};

// Mitsubishi Heavy 152-bit adapter
class Mhi152Adapter : public IAcuAdapter {
public:
  explicit Mhi152Adapter(uint16_t pin = 4);
  void begin() override;
  bool send(const ACUState &state) override;
  const char* name() const override;

private:
  IRMitsubishiHeavy152Ac ir;
};
