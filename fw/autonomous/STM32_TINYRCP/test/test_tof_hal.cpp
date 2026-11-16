// SPDX-License-Identifier: GPL-3.0-only

#include "hal/vl53l0x_hal.hpp"
#include "localization/hal_sensors.hpp"
#include "logging/log.hpp"
#include "navigation/right_hand_rule.hpp"
#include "vl53l0x_api.h"
#include <Arduino.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
namespace {
int64_t clock_us = 1000000;
I2C_HandleTypeDef bus{reinterpret_cast<void *>(1)};
std::map<int, int> pins, modes;
std::vector<tof::LogRecord> records;
unsigned config_modes = 0, gpio_off = 0, starts = 0, polls = 0, offsets = 0,
         xtalk = 0;
int active = -1, fail_init = -1, fail_poll = -1;
bool never_ready = false;
int64_t ready_at = 0;
uint8_t range_status = 0;
uint16_t range_mm = 400;
uint32_t last_timeout = 0;
uint16_t last_address = 0, last_register = 0;
HAL_StatusTypeDef transfer_status = HAL_OK;
int64_t transfer_elapsed = 0;
uint8_t memory[256]{};
unsigned writes = 0, reads = 0;
int id(VL53L0X_DEV d) { return int(d->I2cDevAddr / 2) - 0x30; }
void record(const tof::LogRecord &r, void *) { records.push_back(r); }
tof::Config config() {
  tof::Config c;
  c.i2c = &bus;
  c.log = record;
  c.log_samples = true;
  for (unsigned i = 0; i < 3; ++i)
    c.sensors[i].xshut_pin = 10 + i;
  return c;
}
void reset() {
  tof::stop();
  records.clear();
  pins.clear();
  modes.clear();
  active = fail_init = fail_poll = -1;
  never_ready = false;
  range_status = 0;
  range_mm = 400;
  config_modes = gpio_off = starts = polls = offsets = xtalk = 0;
  clock_us += 1000000;
}
void sample_all(nav::BootstrapFrame *frame = nullptr) {
  for (unsigned i = 0; i < 3; ++i) {
    float value = -1;
    int64_t stamp = -1;
    if (hal_tof_red_distance_corrected(static_cast<loc::TofId>(i), &value,
                                     &stamp) &&
        frame) {
      frame->range_m[i] = value;
      frame->tof_us[i] = stamp;
    }
  }
}
void platform() {
  VL53L0X_Dev_t d{};
  d.I2cHandle = &bus;
  d.I2cDevAddr = 0x60;
  d.i2c_timeout_ms = 5;
  d.deadline_us = clock_us + 1500;
  assert(VL53L0X_WrWord(&d, 0x10, 0x1234) == 0 && memory[0x10] == 0x12 &&
         memory[0x11] == 0x34);
  assert(last_timeout == 2 && last_address == 0x60 && last_register == 0x10);
  uint16_t word = 0;
  assert(VL53L0X_RdWord(&d, 0x10, &word) == 0 && word == 0x1234);
  assert(VL53L0X_WrDWord(&d, 0x20, 0x12345678) == 0);
  uint32_t dword = 0;
  assert(VL53L0X_RdDWord(&d, 0x20, &dword) == 0 && dword == 0x12345678);
  assert(VL53L0X_UpdateByte(&d, 0x10, 0x0f, 0xa0) == 0 && memory[0x10] == 0xa2);
  const auto before = writes;
  uint8_t bytes[4]{};
  assert(VL53L0X_WriteMulti(&d, 254, bytes, 4) ==
             VL53L0X_ERROR_INVALID_PARAMS &&
         writes == before);
  assert(VL53L0X_ReadMulti(nullptr, 0, bytes, 1) ==
         VL53L0X_ERROR_INVALID_PARAMS);
  assert(VL53L0X_ReadMulti(&d, 0, nullptr, 1) == VL53L0X_ERROR_INVALID_PARAMS);
  d.I2cDevAddr = 0x61;
  assert(VL53L0X_WrByte(&d, 0, 1) == VL53L0X_ERROR_INVALID_PARAMS);
  d.I2cDevAddr = 0x60;
  transfer_status = HAL_ERROR;
  word = 0xabcd;
  assert(VL53L0X_RdWord(&d, 0x10, &word) == VL53L0X_ERROR_CONTROL_INTERFACE &&
         word == 0xabcd);
  transfer_status = HAL_TIMEOUT;
  assert(VL53L0X_RdWord(&d, 0x10, &word) == VL53L0X_ERROR_TIME_OUT &&
         word == 0xabcd);
  transfer_status = HAL_OK;
  transfer_elapsed = 2000;
  assert(VL53L0X_RdWord(&d, 0x10, &word) == VL53L0X_ERROR_TIME_OUT &&
         word == 0xabcd);
  transfer_elapsed = 0;
  const auto calls = reads;
  assert(VL53L0X_RdWord(&d, 0x10, &word) == VL53L0X_ERROR_TIME_OUT &&
         reads == calls);
  d.deadline_us = clock_us + 500;
  assert(VL53L0X_PollingDelay(&d) == VL53L0X_ERROR_TIME_OUT);
  assert(VL53L0X_LockSequenceAccess(nullptr) == VL53L0X_ERROR_INVALID_PARAMS);
}
void polling() {
  reset();
  auto c = config();
  c.sensors[0].has_offset_calibration = true;
  c.sensors[0].offset_um = 123;
  c.sensors[0].has_xtalk_calibration = true;
  c.sensors[0].xtalk_mcps_q16 = 456;
  assert(tof::initialize(c));
  assert(config_modes == 3 && gpio_off == 3 && offsets == 1 && xtalk == 1 &&
         starts == 0);
  for (int pin = 10; pin <= 12; ++pin)
    assert(pins[pin] == HIGH && modes[pin] == OUTPUT_OPEN_DRAIN);
  float value = -1;
  int64_t stamp = -1;
  assert(!hal_tof_red_distance_corrected(loc::TofId::LEFT, &value, &stamp));
  assert(starts == 1 && active == 0);
  assert(!tof::read(loc::TofId::LEFT, &value, &stamp) && polls == 0 &&
         value == -1 && stamp == -1);
  clock_us += 33000;
  assert(tof::read(loc::TofId::LEFT, &value, &stamp));
  assert(std::fabs(value - .4f) < 1e-6 && stamp == clock_us);
  assert(!tof::read(loc::TofId::LEFT, &value, &stamp));
  assert(active == 1);
  // Pending LEFT data expires while another sensor is still integrating.
  clock_us += 33000;
  sample_all();
  clock_us += 33000;
  sample_all();
  clock_us += 33000;
  (void)tof::read(loc::TofId::FRONT, &value,
                  &stamp); // Complete LEFT without consuming it.
  never_ready = true;
  clock_us += c.sample_max_age_us + 1;
  value = -7;
  stamp = -7;
  assert(!tof::read(loc::TofId::LEFT, &value, &stamp) && value == -7 &&
         stamp == -7);
  assert(tof::statistics(loc::TofId::LEFT).expired == 1);
  tof::stop();
  for (int pin = 10; pin <= 12; ++pin)
    assert(pins[pin] == LOW);
  assert(!tof::read(loc::TofId::FRONT, &value, &stamp));
  reset();
  assert(tof::initialize(config()));
  for (int i = 0; i < 20; ++i) {
    clock_us += 40000;
    (void)tof::read(loc::TofId::FRONT, &value, &stamp);
  }
  assert(tof::statistics(loc::TofId::LEFT).overwritten > 0);
}
void failures() {
  reset();
  auto c = config();
  c.sensors[1].address_7bit = c.sensors[0].address_7bit;
  assert(!tof::initialize(c) && records.back().event == tof::Event::BAD_CONFIG);
  reset();
  fail_init = 1;
  assert(!tof::initialize(config()));
  for (int pin = 10; pin <= 12; ++pin)
    assert(pins[pin] == LOW);
  reset();
  assert(tof::initialize(config()));
  sample_all();
  fail_poll = 0;
  clock_us += 2000;
  sample_all();
  assert(pins[10] == LOW && !tof::statistics(loc::TofId::LEFT).online);
  assert(tof::statistics(loc::TofId::LEFT).api_errors == 1 && active == 1);
  fail_poll = -1;
  for (int i = 0; i < 8; ++i) {
    clock_us += 40000;
    sample_all();
  }
  assert(tof::statistics(loc::TofId::FRONT).valid > 0 &&
         tof::statistics(loc::TofId::RIGHT).valid > 0);
  reset();
  assert(tof::initialize(config()));
  never_ready = true;
  sample_all();
  clock_us += config().measurement_timeout_us;
  sample_all();
  assert(tof::statistics(loc::TofId::LEFT).timeouts == 1 && pins[10] == LOW);
  for (auto status : {uint8_t(4), uint8_t(0)}) {
    reset();
    assert(tof::initialize(config()));
    range_status = status;
    range_mm = status ? 400 : 0;
    sample_all();
    clock_us += 33000;
    sample_all();
    assert(tof::statistics(loc::TofId::LEFT).rejected == 1);
  }
  reset();
  assert(tof::initialize(config()));
  float value = 7;
  int64_t stamp = 7;
  assert(!tof::read(static_cast<loc::TofId>(99), &value, &stamp) &&
         value == 7 && stamp == 7);
  assert(!tof::read(loc::TofId::LEFT, nullptr, &stamp));
}
void fallback_pipeline() {
  reset();
  assert(tof::initialize(config()));
  nav::RightHandRule rule;
  rule.begin();
  nav::BootstrapFrame frame{};
  nav::DriveRequest drive{};
  bool moved = false;
  // Test the actual sequential HAL -> cached sensor frame -> reactive
  // controller.
  for (int k = 0; k < 200; ++k) {
    clock_us += 10000;
    if (k % 5 == 0)
      sample_all(&frame); // Production 50 ms ToF poll cadence.
    frame.imu_us = clock_us;
    motor::MotorSnapshot m{};
    m.timestamp_us = clock_us;
    if (k % 2 == 0) {
      // Fresh independent heading projects wall ranges for cell centering.
      const nav::HeadingObservation heading{0, clock_us, true};
      drive = rule.update(frame, m, clock_us, nullptr, &heading);
    } // Production 20 ms path cadence.
    moved = moved || drive.linear_m_s != 0 || drive.angular_rad_s != 0;
  }
  assert(moved);
  fail_poll = 1;
  for (int k = 0; k < 60; ++k) {
    clock_us += 10000;
    if (k % 5 == 0)
      sample_all(&frame);
    frame.imu_us = clock_us;
    motor::MotorSnapshot m{};
    m.timestamp_us = clock_us;
    if (k % 2 == 0)
      drive = rule.update(frame, m, clock_us);
  }
  assert(!tof::statistics(loc::TofId::FRONT).online);
  assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
}
} // namespace
int64_t hal_time_us() { return clock_us; }
void digitalWrite(int pin, int value) { pins[pin] = value; }
void pinMode(int pin, int value) { modes[pin] = value; }
void delayMicroseconds(unsigned int us) { clock_us += us; }
namespace robot_log {
bool write(Level, const char *, const char *, ...) { return true; }
bool writev(Level, const char *, const char *, va_list) { return true; }
} // namespace robot_log
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t a,
                                        uint32_t, uint32_t timeout) {
  assert(a == 0x52 && timeout > 0);
  return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *, uint16_t a,
                                    uint16_t reg, uint16_t width, uint8_t *data,
                                    uint16_t n, uint32_t timeout) {
  assert(width == I2C_MEMADD_SIZE_8BIT && timeout > 0);
  last_address = a;
  last_register = reg;
  last_timeout = timeout;
  ++writes;
  std::memcpy(memory + reg, data, n);
  clock_us += transfer_elapsed;
  return transfer_status;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *, uint16_t a,
                                   uint16_t reg, uint16_t width, uint8_t *data,
                                   uint16_t n, uint32_t timeout) {
  assert(width == I2C_MEMADD_SIZE_8BIT && timeout > 0);
  last_address = a;
  last_register = reg;
  last_timeout = timeout;
  ++reads;
  std::memcpy(data, memory + reg, n);
  clock_us += transfer_elapsed;
  return transfer_status;
}
extern "C" {
VL53L0X_Error VL53L0X_SetDeviceAddress(VL53L0X_DEV d, uint8_t a) {
  assert(d->I2cDevAddr == 0x52 && a >= 0x60 && a <= 0x64);
  return 0;
}
VL53L0X_Error VL53L0X_DataInit(VL53L0X_DEV d) {
  return id(d) == fail_init ? VL53L0X_ERROR_CONTROL_INTERFACE : 0;
}
VL53L0X_Error VL53L0X_StaticInit(VL53L0X_DEV) { return 0; }
VL53L0X_Error VL53L0X_PerformRefSpadManagement(VL53L0X_DEV, uint32_t *s,
                                               uint8_t *a) {
  *s = 1;
  *a = 0;
  return 0;
}
VL53L0X_Error VL53L0X_PerformRefCalibration(VL53L0X_DEV, uint8_t *v,
                                            uint8_t *p) {
  *v = *p = 0;
  return 0;
}
VL53L0X_Error VL53L0X_SetOffsetCalibrationDataMicroMeter(VL53L0X_DEV,
                                                         int32_t v) {
  assert(v == 123);
  ++offsets;
  return 0;
}
VL53L0X_Error VL53L0X_SetXTalkCompensationRateMegaCps(VL53L0X_DEV,
                                                      FixPoint1616_t v) {
  assert(v == 456);
  ++xtalk;
  return 0;
}
VL53L0X_Error VL53L0X_SetXTalkCompensationEnable(VL53L0X_DEV, uint8_t) {
  return 0;
}
VL53L0X_Error VL53L0X_SetDeviceMode(VL53L0X_DEV, uint8_t mode) {
  assert(mode == VL53L0X_DEVICEMODE_SINGLE_RANGING);
  ++config_modes;
  return 0;
}
VL53L0X_Error VL53L0X_SetMeasurementTimingBudgetMicroSeconds(VL53L0X_DEV,
                                                             uint32_t b) {
  assert(b == 33000);
  return 0;
}
VL53L0X_Error VL53L0X_SetGpioConfig(VL53L0X_DEV, uint8_t pin, uint8_t mode,
                                    uint8_t function, uint8_t polarity) {
  assert(pin == 0 && mode == VL53L0X_DEVICEMODE_SINGLE_RANGING &&
         function == VL53L0X_GPIOFUNCTIONALITY_OFF &&
         polarity == VL53L0X_INTERRUPTPOLARITY_LOW);
  ++gpio_off;
  return 0;
}
VL53L0X_Error VL53L0X_ClearInterruptMask(VL53L0X_DEV d, uint32_t mask) {
  assert(mask == 0);
  if (active == id(d))
    active = -1;
  return 0;
}
VL53L0X_Error VL53L0X_StartMeasurement(VL53L0X_DEV d) {
  // Faults XSHUT-isolate the previous active module before starting the next
  // one.
  if (active >= 0)
    assert(pins[10 + active] == LOW);
  active = id(d);
  ready_at = clock_us + 33000;
  ++starts;
  assert(d->deadline_us > clock_us);
  return 0;
}
VL53L0X_Error VL53L0X_GetMeasurementDataReady(VL53L0X_DEV d, uint8_t *ready) {
  ++polls;
  assert(active == id(d));
  if (id(d) == fail_poll)
    return VL53L0X_ERROR_CONTROL_INTERFACE;
  *ready = !never_ready && clock_us >= ready_at;
  return 0;
}
VL53L0X_Error
VL53L0X_GetRangingMeasurementData(VL53L0X_DEV d,
                                  VL53L0X_RangingMeasurementData_t *out) {
  assert(active == id(d) && clock_us >= ready_at);
  out->RangeStatus = range_status;
  out->RangeMilliMeter = range_mm;
  return 0;
}
}
int main() {
  platform();
  polling();
  failures();
  fallback_pipeline();
  tof::stop();
  std::puts("PASS real ToF HAL/platform: polling-only, sequential samples, "
            "calibration, freshness, faults, deadlines, endian and fallback "
            "pipeline");
}
