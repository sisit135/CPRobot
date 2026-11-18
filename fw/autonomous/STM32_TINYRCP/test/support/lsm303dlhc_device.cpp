// SPDX-License-Identifier: GPL-3.0-only
#include "lsm303dlhc_device.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace lsm_fake {
Device device;
void Device::reset() {
  *this = Device{};
  bus.Instance = I2C1;
  accel[0x0F] = 0x33;
  mag[0x0A] = 0x48;
  mag[0x0B] = 0x34;
  mag[0x0C] = 0x33;
}
void Device::acceleration(float x, float y, float z) {
  const float values[] = {x, y, z};
  for (unsigned i = 0; i < 3; ++i) {
    const int raw = std::clamp(
        int(std::lround(values[i] / (0.002f * 9.80665f))), -2048, 2047);
    const uint16_t word = static_cast<uint16_t>(raw * 16);
    accel[0x28 + 2 * i] = word & 0xFF;
    accel[0x29 + 2 * i] = word >> 8;
  }
  accel[0x27] = 8;
}
void Device::magnetic(float x, float y, float z) {
  const float values[] = {x, z, y}; // Silicon wire order X,Z,Y.
  for (unsigned i = 0; i < 3; ++i) {
    const float gain = i == 1 ? 9.8f : 11.0f;
    const auto word = static_cast<uint16_t>(static_cast<int16_t>(
        std::clamp(int(std::lround(values[i] * gain)), -4096, 4095)));
    mag[3 + 2 * i] = word >> 8;
    mag[4 + 2 * i] = word & 0xFF;
  }
  mag[9] = 1;
}
void Device::sample(int64_t now, const float *a, const float *m) {
  if (accel[0x20] && now / 10000 != accel_epoch) {
    accel_epoch = now / 10000;
    if (!(accel[0x27] & 8)) // Acceleration BDU retains an unread frame.
      acceleration(a[0], a[1], a[2]);
  }
  if (mag[2] == 0 && now * 220 / 1000000 != mag_epoch) {
    mag_epoch = now * 220 / 1000000;
    if (!(mag[9] & 2)) // A partial MAG frame locks all axes until drained.
      magnetic(m[0], m[1], m[2]);
  }
}
} // namespace lsm_fake

namespace {
bool fail() {
  auto &d = lsm_fake::device;
  if (d.before_transfer)
    d.before_transfer();
  if (d.advance_time)
    d.advance_time(d.latency_us);
  return ++d.transfers == d.fail_at;
}
} // namespace
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *) { return 4; }
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *bus, uint16_t address,
                                    uint16_t reg, uint16_t size, uint8_t *data,
                                    uint16_t count, uint32_t timeout) {
  auto &d = lsm_fake::device;
  if (fail() || bus != &d.bus || size != I2C_MEMADD_SIZE_8BIT || !timeout ||
      (address != 0x32 && address != 0x3C) || count != 1)
    return HAL_ERROR;
  auto &regs = address == 0x32 ? d.accel : d.mag;
  regs[reg] = *data;
  return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus, uint16_t address,
                                   uint16_t reg, uint16_t size, uint8_t *data,
                                   uint16_t count, uint32_t timeout) {
  auto &d = lsm_fake::device;
  if (fail() || bus != &d.bus || size != I2C_MEMADD_SIZE_8BIT || !timeout ||
      (address != 0x32 && address != 0x3C) || !count || count > 8)
    return HAL_ERROR;
  const bool accel = address == 0x32;
  if ((accel && count > 1 && !(reg & 0x80)) || (!accel && (reg & 0x80)))
    return HAL_ERROR;
  reg &= 0x7F;
  if (reg + count > 256)
    return HAL_ERROR;
  auto &regs = accel ? d.accel : d.mag;
  std::memcpy(data, regs.data() + reg, count);
  if (d.corrupt_readback && accel && reg == 0x23)
    data[0] ^= 0x10;
  if (accel && reg <= 0x28 && reg + count > 0x2D)
    regs[0x27] = 0;
  if (!accel && reg == 3)
    regs[9] = count == 6 ? 0 : 3;
  return HAL_OK;
}
