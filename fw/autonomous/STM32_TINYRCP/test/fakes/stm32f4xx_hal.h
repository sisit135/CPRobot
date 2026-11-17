// SPDX-License-Identifier: GPL-3.0-only

// Host declarations only. Never use this directory in firmware include paths.
#pragma once
#include <cstdint>
struct I2C_HandleTypeDef {
  void *Instance = nullptr;
};
// Distinct controller identities for diagnostic routing; never dereferenced.
#define I2C1 (reinterpret_cast<void *>(0x40005400))
#define I2C2 (reinterpret_cast<void *>(0x40005800))
#define I2C3 (reinterpret_cast<void *>(0x40005c00))
enum HAL_StatusTypeDef {
  HAL_OK = 0,
  HAL_ERROR = 1,
  HAL_BUSY = 2,
  HAL_TIMEOUT = 3
};
constexpr uint16_t I2C_MEMADD_SIZE_8BIT = 1;
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t, uint32_t,
                                        uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *, uint16_t, uint16_t,
                                    uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *, uint16_t, uint16_t,
                                   uint16_t, uint8_t *, uint16_t, uint32_t);
