// Host declarations only. Never use this directory in firmware include paths.
#pragma once
#include <cstdint>
struct I2C_HandleTypeDef {
  void *Instance = nullptr;
};
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
