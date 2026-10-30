// Narrow C API facade over the bundled ST VL53L0X C++ vendor driver.
#pragma once
#include "vl53l0x_platform.h"
#ifdef __cplusplus
extern "C" {
#endif
VL53L0X_Error VL53L0X_SetDeviceAddress(VL53L0X_DEV, uint8_t);
VL53L0X_Error VL53L0X_DataInit(VL53L0X_DEV);
VL53L0X_Error VL53L0X_StaticInit(VL53L0X_DEV);
VL53L0X_Error VL53L0X_PerformRefSpadManagement(VL53L0X_DEV, uint32_t *,
                                               uint8_t *);
VL53L0X_Error VL53L0X_PerformRefCalibration(VL53L0X_DEV, uint8_t *, uint8_t *);
VL53L0X_Error VL53L0X_SetOffsetCalibrationDataMicroMeter(VL53L0X_DEV, int32_t);
VL53L0X_Error VL53L0X_SetXTalkCompensationRateMegaCps(VL53L0X_DEV,
                                                      FixPoint1616_t);
VL53L0X_Error VL53L0X_SetXTalkCompensationEnable(VL53L0X_DEV, uint8_t);
VL53L0X_Error VL53L0X_SetDeviceMode(VL53L0X_DEV, uint8_t);
VL53L0X_Error VL53L0X_SetMeasurementTimingBudgetMicroSeconds(VL53L0X_DEV,
                                                             uint32_t);
VL53L0X_Error VL53L0X_SetGpioConfig(VL53L0X_DEV, uint8_t, uint8_t, uint8_t,
                                    uint8_t);
VL53L0X_Error VL53L0X_ClearInterruptMask(VL53L0X_DEV, uint32_t);
VL53L0X_Error VL53L0X_StartMeasurement(VL53L0X_DEV);
VL53L0X_Error VL53L0X_GetMeasurementDataReady(VL53L0X_DEV, uint8_t *);
VL53L0X_Error
VL53L0X_GetRangingMeasurementData(VL53L0X_DEV,
                                  VL53L0X_RangingMeasurementData_t *);
#ifdef __cplusplus
}
#endif
