// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "stm32f4xx_hal.h"
#include <array>
#include <cstdint>

// Host register model. Quantizes independent ACCEL/MAG measurements; it has no
// access to controller state or gyro data. Does not model electrical timing.
namespace lsm_fake {
struct Device {
  std::array<uint8_t, 256> accel{}, mag{};
  I2C_HandleTypeDef bus{};
  unsigned transfers = 0, fail_at = 0;
  bool corrupt_readback = false;
  int64_t accel_epoch = -1, mag_epoch = -1;
  uint32_t latency_us = 0;
  void (*advance_time)(int64_t) = nullptr;
  void (*before_transfer)() = nullptr;
  void reset();
  void acceleration(float x, float y, float z);
  void magnetic(float x, float y, float z);
  void sample(int64_t now, const float *acceleration, const float *field);
};
extern Device device;
} // namespace lsm_fake
