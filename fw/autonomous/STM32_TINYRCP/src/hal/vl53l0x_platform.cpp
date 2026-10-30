#include "vl53l0x_platform.h"
#include "platform/hal_platform.hpp"
#include <Arduino.h>
#include <algorithm>

namespace {
VL53L0X_Error parameters(VL53L0X_DEV dev, const void *data, uint8_t reg,
                         uint32_t count) {
  if (!dev || !dev->I2cHandle || !dev->I2cHandle->Instance || !data || !count ||
      count > 256u - reg || !dev->i2c_timeout_ms || dev->I2cDevAddr < 0x10 ||
      dev->I2cDevAddr > 0xEE || (dev->I2cDevAddr & 1u))
    return VL53L0X_ERROR_INVALID_PARAMS;
  if (dev->deadline_us && hal_time_us() >= dev->deadline_us)
    return VL53L0X_ERROR_TIME_OUT;
  return VL53L0X_ERROR_NONE;
}
uint32_t timeout_ms(VL53L0X_DEV dev) {
  if (!dev->deadline_us)
    return dev->i2c_timeout_ms;
  const int64_t remaining = dev->deadline_us - hal_time_us();
  if (remaining <= 0)
    return 0;
  return std::min<uint32_t>(dev->i2c_timeout_ms,
                            static_cast<uint32_t>((remaining + 999) / 1000));
}
VL53L0X_Error convert(HAL_StatusTypeDef value) {
  if (value == HAL_OK)
    return VL53L0X_ERROR_NONE;
  return value == HAL_TIMEOUT ? VL53L0X_ERROR_TIME_OUT
                              : VL53L0X_ERROR_CONTROL_INTERFACE;
}
} // namespace
extern "C" {
VL53L0X_Error VL53L0X_LockSequenceAccess(VL53L0X_DEV dev) {
  return dev && dev->I2cHandle ? VL53L0X_ERROR_NONE
                               : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_UnlockSequenceAccess(VL53L0X_DEV dev) {
  return dev ? VL53L0X_ERROR_NONE : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_WriteMulti(VL53L0X_DEV dev, uint8_t reg, uint8_t *data,
                                 uint32_t count) {
  const auto error = parameters(dev, data, reg, count);
  if (error != VL53L0X_ERROR_NONE)
    return error;
  const uint32_t ms = timeout_ms(dev);
  if (!ms)
    return VL53L0X_ERROR_TIME_OUT;
  const auto result = convert(HAL_I2C_Mem_Write(
      dev->I2cHandle, dev->I2cDevAddr, reg, I2C_MEMADD_SIZE_8BIT, data,
      static_cast<uint16_t>(count), ms));
  if (dev->deadline_us && hal_time_us() >= dev->deadline_us)
    return VL53L0X_ERROR_TIME_OUT;
  return result;
}
VL53L0X_Error VL53L0X_ReadMulti(VL53L0X_DEV dev, uint8_t reg, uint8_t *data,
                                uint32_t count) {
  const auto error = parameters(dev, data, reg, count);
  if (error != VL53L0X_ERROR_NONE)
    return error;
  const uint32_t ms = timeout_ms(dev);
  if (!ms)
    return VL53L0X_ERROR_TIME_OUT;
  const auto result = convert(HAL_I2C_Mem_Read(
      dev->I2cHandle, dev->I2cDevAddr, reg, I2C_MEMADD_SIZE_8BIT, data,
      static_cast<uint16_t>(count), ms));
  if (dev->deadline_us && hal_time_us() >= dev->deadline_us)
    return VL53L0X_ERROR_TIME_OUT;
  return result;
}
VL53L0X_Error VL53L0X_WrByte(VL53L0X_DEV dev, uint8_t reg, uint8_t data) {
  return VL53L0X_WriteMulti(dev, reg, &data, 1);
}
VL53L0X_Error VL53L0X_WrWord(VL53L0X_DEV dev, uint8_t reg, uint16_t data) {
  uint8_t b[] = {static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data)};
  return VL53L0X_WriteMulti(dev, reg, b, 2);
}
VL53L0X_Error VL53L0X_WrDWord(VL53L0X_DEV dev, uint8_t reg, uint32_t data) {
  uint8_t b[] = {static_cast<uint8_t>(data >> 24),
                 static_cast<uint8_t>(data >> 16),
                 static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data)};
  return VL53L0X_WriteMulti(dev, reg, b, 4);
}
VL53L0X_Error VL53L0X_RdByte(VL53L0X_DEV dev, uint8_t reg, uint8_t *data) {
  if (!data)
    return VL53L0X_ERROR_INVALID_PARAMS;
  uint8_t value = 0;
  const auto status = VL53L0X_ReadMulti(dev, reg, &value, 1);
  if (status == VL53L0X_ERROR_NONE)
    *data = value;
  return status;
}
VL53L0X_Error VL53L0X_RdWord(VL53L0X_DEV dev, uint8_t reg, uint16_t *data) {
  if (!data)
    return VL53L0X_ERROR_INVALID_PARAMS;
  uint8_t b[2]{};
  const auto status = VL53L0X_ReadMulti(dev, reg, b, 2);
  if (status == VL53L0X_ERROR_NONE)
    *data = static_cast<uint16_t>((uint16_t(b[0]) << 8) | b[1]);
  return status;
}
VL53L0X_Error VL53L0X_RdDWord(VL53L0X_DEV dev, uint8_t reg, uint32_t *data) {
  if (!data)
    return VL53L0X_ERROR_INVALID_PARAMS;
  uint8_t b[4]{};
  const auto status = VL53L0X_ReadMulti(dev, reg, b, 4);
  if (status == VL53L0X_ERROR_NONE)
    *data = (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) |
            (uint32_t(b[2]) << 8) | b[3];
  return status;
}
VL53L0X_Error VL53L0X_UpdateByte(VL53L0X_DEV dev, uint8_t reg, uint8_t and_mask,
                                 uint8_t or_mask) {
  uint8_t value = 0;
  const auto status = VL53L0X_RdByte(dev, reg, &value);
  if (status != VL53L0X_ERROR_NONE)
    return status;
  return VL53L0X_WrByte(dev, reg,
                        static_cast<uint8_t>((value & and_mask) | or_mask));
}
VL53L0X_Error VL53L0X_PollingDelay(VL53L0X_DEV dev) {
  if (!dev)
    return VL53L0X_ERROR_INVALID_PARAMS;
  if (dev->deadline_us && hal_time_us() >= dev->deadline_us)
    return VL53L0X_ERROR_TIME_OUT;
  delayMicroseconds(1000);
  return dev->deadline_us && hal_time_us() >= dev->deadline_us
             ? VL53L0X_ERROR_TIME_OUT
             : VL53L0X_ERROR_NONE;
}
}
