// SPDX-License-Identifier: GPL-3.0-only

// CPRobot bridge to the bundled ST C++ implementation. No second ST core.
#include "vendor/vl53l0x_class.h"
#include "vl53l0x_api.h"
namespace {
class VendorPort final : public VL53L0X {
public:
  VendorPort() : VL53L0X(nullptr, -1) {}
  void bind(VL53L0X_DEV dev) { Device = dev; }
  using VL53L0X::VL53L0X_ClearInterruptMask;
  using VL53L0X::VL53L0X_DataInit;
  using VL53L0X::VL53L0X_GetMeasurementDataReady;
  using VL53L0X::VL53L0X_GetRangingMeasurementData;
  using VL53L0X::VL53L0X_PerformRefCalibration;
  using VL53L0X::VL53L0X_PerformRefSpadManagement;
  using VL53L0X::VL53L0X_SetDeviceAddress;
  using VL53L0X::VL53L0X_SetDeviceMode;
  using VL53L0X::VL53L0X_SetGpioConfig;
  using VL53L0X::VL53L0X_SetMeasurementTimingBudgetMicroSeconds;
  using VL53L0X::VL53L0X_SetOffsetCalibrationDataMicroMeter;
  using VL53L0X::VL53L0X_StartMeasurement;
  using VL53L0X::VL53L0X_StaticInit;
};
VendorPort *port(VL53L0X_DEV dev) {
  // All calls belong to the single sensor task. Three static slots, no heap.
  static VendorPort ports[3];
  static VL53L0X_DEV owners[3]{};
  if (!dev || !dev->I2cHandle || !dev->I2cHandle->Instance)
    return nullptr;
  for (unsigned i = 0; i < 3; ++i)
    if (owners[i] == dev)
      return &ports[i];
  for (unsigned i = 0; i < 3; ++i)
    if (!owners[i]) {
      owners[i] = dev;
      ports[i].bind(dev);
      return &ports[i];
    }
  return nullptr;
}
} // namespace
extern "C" {
VL53L0X_Error VL53L0X_SetDeviceAddress(VL53L0X_DEV dev, uint8_t address) {
  auto *p = port(dev);
  return p ? p->VL53L0X_SetDeviceAddress(dev, address)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_DataInit(VL53L0X_DEV dev) {
  auto *p = port(dev);
  return p ? p->VL53L0X_DataInit(dev) : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_StaticInit(VL53L0X_DEV dev) {
  auto *p = port(dev);
  return p ? p->VL53L0X_StaticInit(dev) : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_PerformRefSpadManagement(VL53L0X_DEV dev, uint32_t *count,
                                               uint8_t *aperture) {
  auto *p = port(dev);
  return p ? p->VL53L0X_PerformRefSpadManagement(dev, count, aperture)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_PerformRefCalibration(VL53L0X_DEV dev, uint8_t *vhv,
                                            uint8_t *phase) {
  auto *p = port(dev);
  return p ? p->VL53L0X_PerformRefCalibration(dev, vhv, phase)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_SetOffsetCalibrationDataMicroMeter(VL53L0X_DEV dev,
                                                         int32_t offset) {
  auto *p = port(dev);
  return p ? p->VL53L0X_SetOffsetCalibrationDataMicroMeter(dev, offset)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
// This bundled vendor revision exposes xtalk getters but omits setters.
// Use its named register, Q3.13/Q16.16 conversion and cached parameter fields.
// Only the vendor's native (gain=1000) hardware-correction path is supported.
VL53L0X_Error VL53L0X_SetXTalkCompensationRateMegaCps(VL53L0X_DEV dev,
                                                      FixPoint1616_t rate) {
  if (!port(dev) || (rate >> 3) > 0xffffu)
    return VL53L0X_ERROR_INVALID_PARAMS;
  if (dev->Data.LinearityCorrectiveGain != 1000)
    return VL53L0X_ERROR_NOT_SUPPORTED;
  if (dev->Data.CurrentParameters.XTalkCompensationEnable) {
    const auto result =
        VL53L0X_WrWord(dev, VL53L0X_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS,
                       VL53L0X_FIXPOINT1616TOFIXPOINT313(rate));
    if (result)
      return result;
  }
  dev->Data.CurrentParameters.XTalkCompensationRateMegaCps = rate;
  return VL53L0X_ERROR_NONE;
}
VL53L0X_Error VL53L0X_SetXTalkCompensationEnable(VL53L0X_DEV dev,
                                                 uint8_t enable) {
  if (!port(dev) || enable > 1)
    return VL53L0X_ERROR_INVALID_PARAMS;
  if (enable && dev->Data.LinearityCorrectiveGain != 1000)
    return VL53L0X_ERROR_NOT_SUPPORTED;
  const auto rate = dev->Data.CurrentParameters.XTalkCompensationRateMegaCps;
  if (enable && (rate >> 3) > 0xffffu)
    return VL53L0X_ERROR_INVALID_PARAMS;
  const auto result =
      VL53L0X_WrWord(dev, VL53L0X_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS,
                     enable ? VL53L0X_FIXPOINT1616TOFIXPOINT313(rate) : 0);
  if (!result)
    dev->Data.CurrentParameters.XTalkCompensationEnable = enable;
  return result;
}
VL53L0X_Error VL53L0X_SetDeviceMode(VL53L0X_DEV dev, uint8_t mode) {
  auto *p = port(dev);
  return p ? p->VL53L0X_SetDeviceMode(dev, mode) : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_SetMeasurementTimingBudgetMicroSeconds(VL53L0X_DEV dev,
                                                             uint32_t budget) {
  auto *p = port(dev);
  return p ? p->VL53L0X_SetMeasurementTimingBudgetMicroSeconds(dev, budget)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_SetGpioConfig(VL53L0X_DEV dev, uint8_t pin, uint8_t mode,
                                    uint8_t function, uint8_t polarity) {
  auto *p = port(dev);
  return p ? p->VL53L0X_SetGpioConfig(dev, pin, mode, function, polarity)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_ClearInterruptMask(VL53L0X_DEV dev, uint32_t mask) {
  auto *p = port(dev);
  return p ? p->VL53L0X_ClearInterruptMask(dev, mask)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_StartMeasurement(VL53L0X_DEV dev) {
  auto *p = port(dev);
  return p ? p->VL53L0X_StartMeasurement(dev) : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error VL53L0X_GetMeasurementDataReady(VL53L0X_DEV dev, uint8_t *ready) {
  auto *p = port(dev);
  return p ? p->VL53L0X_GetMeasurementDataReady(dev, ready)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
VL53L0X_Error
VL53L0X_GetRangingMeasurementData(VL53L0X_DEV dev,
                                  VL53L0X_RangingMeasurementData_t *data) {
  auto *p = port(dev);
  return p ? p->VL53L0X_GetRangingMeasurementData(dev, data)
           : VL53L0X_ERROR_INVALID_PARAMS;
}
}
