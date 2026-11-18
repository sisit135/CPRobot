// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "stm32f4xx_hal.h"
#include <cstddef>
#include <cstdint>

namespace i2c_diagnostics {
struct Binding {
  uint8_t address; // Seven-bit configured address; not an identity assertion.
  const char *device;
};
// BUP task only, before sensor services start. Address-only probes, no register
// access or address assignment. A scan deadline bounds retries on a stuck bus.
void scan(I2C_HandleTypeDef *bus, const char *name, const char *phase,
          const Binding *bindings, std::size_t count);
// Address-only open-drain GPIO master, before Wire/controller initialization.
// Requires external pull-ups. Does not recover a stuck bus or change XSHUT.
// Always releases both pins, including after a stretch timeout.
void scan_gpio(int sda, int scl, const char *name, const Binding *bindings,
               std::size_t count);
// Capture physical lines and controller state without clearing status flags.
void state(I2C_HandleTypeDef *bus, int sda, int scl, const char *phase);
} // namespace i2c_diagnostics
