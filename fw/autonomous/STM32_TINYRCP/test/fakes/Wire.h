// SPDX-License-Identifier: GPL-3.0-only

// Test bus surface; not a STM32 SDK.
#pragma once
#include "stm32f4xx_hal.h"
#include <cstddef>
#include <cstdint>
struct FakeWire {
  FakeWire() = default;
  FakeWire(uint32_t, uint32_t) { bus.Instance = I2C2; }
  I2C_HandleTypeDef bus{I2C1};
  I2C_HandleTypeDef *getHandle() {
    return &bus;
  }
  uint8_t registers[256]{}, pointer = 0, command = 0;
  unsigned written = 0, remaining = 0;
  bool fail = false;
  void begin() {}
  void setSDA(uint32_t) {}
  void setSCL(uint32_t) {}
  void setClock(uint32_t) {}
  void beginTransmission(uint8_t) { written = 0; }
  std::size_t write(uint8_t value) {
    if (written++ == 0) {
      command = value;
      pointer = value & 0x1f;
    } else
      registers[pointer++] = value;
    return 1;
  }
  uint8_t endTransmission(bool = true) { return fail ? 4 : 0; }
  uint8_t requestFrom(uint8_t, uint8_t count) {
    remaining = fail ? 0 : count;
    return remaining;
  }
  int available() { return remaining; }
  int read() {
    if (!remaining)
      return -1;
    --remaining;
    return registers[pointer++];
  }
};
extern FakeWire Wire;

using TwoWire = FakeWire;
