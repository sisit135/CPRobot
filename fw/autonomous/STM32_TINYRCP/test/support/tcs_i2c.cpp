// SPDX-License-Identifier: GPL-3.0-only
#include "Wire.h"
#include "hal/sensors_config.hpp"
#include <cassert>

namespace {
void check(I2C_HandleTypeDef *bus, uint16_t address, uint16_t reg,
           uint16_t size, uint16_t count, uint32_t timeout) {
  assert(bus == Wire.getHandle());
  assert(address == (board::sensors::kTcs34725Address7bit << 1));
  assert((reg & 0x80) && size == I2C_MEMADD_SIZE_8BIT);
  assert(count >= 1 && count <= 8);
  assert(count == 1 || (reg & 0x20)); // Auto-increment for an RGBC frame.
  assert(timeout == board::sensors::kI2cTimeoutMs);
}
} // namespace
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *) { return Wire.fail ? 4 : 0; }
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus, uint16_t address,
                                   uint16_t reg, uint16_t size, uint8_t *data,
                                   uint16_t count, uint32_t timeout) {
  check(bus, address, reg, size, count, timeout);
  if (Wire.fail)
    return HAL_ERROR;
  for (unsigned i = 0; i < count; ++i)
    data[i] = Wire.registers[(reg & 0x1f) + i];
  return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *bus, uint16_t address,
                                    uint16_t reg, uint16_t size, uint8_t *data,
                                    uint16_t count, uint32_t timeout) {
  check(bus, address, reg, size, count, timeout);
  if (Wire.fail)
    return HAL_ERROR;
  for (unsigned i = 0; i < count; ++i)
    Wire.registers[(reg & 0x1f) + i] = data[i];
  return HAL_OK;
}
