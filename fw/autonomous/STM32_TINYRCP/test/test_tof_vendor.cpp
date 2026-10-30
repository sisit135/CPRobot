// Links the real HAL + C facade + supplied ST C++ algorithms + STM32 port.
#include "hal/vl53l0x_hal.hpp"
#include "logging/log.hpp"
#include "vl53l0x_api.h"
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
namespace {
int64_t now = 1000000;
I2C_HandleTypeDef bus{reinterpret_cast<void *>(1)};
uint8_t registers[3][256]{};
unsigned transfers = 0;
bool fail = false;
unsigned fail_after = UINT32_MAX;
std::map<int, int> pins;
} // namespace
int64_t hal_time_us() { return now; }
void delayMicroseconds(unsigned int us) { now += us; }
void delay(unsigned long ms) { now += ms * 1000; }
void pinMode(int, int) {}
void digitalWrite(int p, int v) { pins[p] = v; }
namespace robot_log {
bool write(Level, const char *, const char *, ...) { return true; }
bool writev(Level, const char *, const char *, va_list) { return true; }
} // namespace robot_log
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t, uint32_t,
                                        uint32_t) {
  return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *, uint16_t address,
                                    uint16_t reg, uint16_t, uint8_t *data,
                                    uint16_t n, uint32_t timeout) {
  assert(timeout > 0 && address >= 0x52 && address <= 0x64);
  ++transfers;
  if (fail || transfers > fail_after)
    return HAL_ERROR;
  const int i = address < 0x60 ? 0 : (address - 0x60) / 2;
  std::memcpy(registers[i] + reg, data, n);
  return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *, uint16_t address,
                                   uint16_t reg, uint16_t, uint8_t *data,
                                   uint16_t n, uint32_t timeout) {
  assert(timeout > 0 && address >= 0x52 && address <= 0x64);
  ++transfers;
  if (fail || transfers > fail_after)
    return HAL_ERROR;
  const int i = address < 0x60 ? 0 : (address - 0x60) / 2;
  std::memcpy(data, registers[i] + reg, n);
  return HAL_OK;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  if (std::string(argv[1]) == "init_failure") {
    tof::Config c;
    c.i2c = &bus;
    for (unsigned i = 0; i < 3; ++i)
      c.sensors[i].xshut_pin = 10 + i;
    fail_after = 1;
    assert(!tof::initialize(
        c)); // Real vendor DataInit propagates the bus failure.
    for (int pin = 10; pin <= 12; ++pin)
      assert(pins[pin] == LOW);
    assert(transfers > 1 && tof::statistics(loc::TofId::LEFT).api_errors == 1);
    std::puts(
        "PASS actual vendor init failure -> HAL logs/error -> all XSHUT low");
    return 0;
  }
  VL53L0X_Dev_t dev[3]{};
  for (unsigned i = 0; i < 3; ++i) {
    auto &d = dev[i];
    d.I2cHandle = &bus;
    d.I2cDevAddr = 0x52;
    d.i2c_timeout_ms = 5;
    d.deadline_us = now + 50000;
    assert(VL53L0X_SetDeviceAddress(&d, uint8_t(0x60 + 2 * i)) == 0);
    assert(registers[0][0x8a] == 0x30 + i);
    d.I2cDevAddr = 0x60 + 2 * i;
    assert(VL53L0X_SetDeviceMode(&d, VL53L0X_DEVICEMODE_SINGLE_RANGING) == 0);
    assert(VL53L0X_SetGpioConfig(&d, 0, VL53L0X_DEVICEMODE_SINGLE_RANGING,
                                 VL53L0X_GPIOFUNCTIONALITY_OFF,
                                 VL53L0X_INTERRUPTPOLARITY_LOW) == 0);
    assert(registers[i][VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG_GPIO] == 0);
    registers[i][VL53L0X_REG_RESULT_RANGE_STATUS] = 1;
    uint8_t ready = 0;
    assert(VL53L0X_GetMeasurementDataReady(&d, &ready) == 0 && ready == 1);
    registers[i][VL53L0X_REG_RESULT_RANGE_STATUS] = 0;
    assert(VL53L0X_GetMeasurementDataReady(&d, &ready) == 0 && ready == 0);
    fail = true;
    ready = 9;
    assert(VL53L0X_GetMeasurementDataReady(&d, &ready) ==
               VL53L0X_ERROR_CONTROL_INTERFACE &&
           ready == 9);
    fail = false;
    d.deadline_us = now;
    const auto before = transfers;
    assert(VL53L0X_GetMeasurementDataReady(&d, &ready) ==
               VL53L0X_ERROR_TIME_OUT &&
           transfers == before);
    d.deadline_us = now + 50000;
    d.Data.LinearityCorrectiveGain = 1000;
    assert(VL53L0X_SetXTalkCompensationRateMegaCps(&d, 456) == 0);
    assert(d.Data.CurrentParameters.XTalkCompensationRateMegaCps == 456);
    assert(VL53L0X_SetXTalkCompensationEnable(&d, 1) == 0);
    assert(registers[i][0x20] == 0 && registers[i][0x21] == 57);
    assert(VL53L0X_SetXTalkCompensationEnable(&d, 0) == 0 &&
           registers[i][0x21] == 0);
    assert(VL53L0X_SetXTalkCompensationRateMegaCps(&d, 0x100000) ==
           VL53L0X_ERROR_INVALID_PARAMS);
    d.Data.LinearityCorrectiveGain = 900;
    assert(VL53L0X_SetXTalkCompensationEnable(&d, 1) ==
           VL53L0X_ERROR_NOT_SUPPORTED);
  }
  assert(VL53L0X_StartMeasurement(nullptr) == VL53L0X_ERROR_INVALID_PARAMS);
  std::puts("PASS actual ST vendor facade: three contexts, address encoding, "
            "GPIO OFF, readiness polling, bus errors and deadlines");
}
