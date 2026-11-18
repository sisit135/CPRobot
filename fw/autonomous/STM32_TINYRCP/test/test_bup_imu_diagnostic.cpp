// SPDX-License-Identifier: GPL-3.0-only
#include "Wire.h"
#include "bup/bup_tasks.hpp"
#include "hal/boot_sensor_sample.hpp"
#include "hal/i2c_diagnostics.hpp"
#include "hal/mpu9250_hal.hpp"
#include "hal/vl53l0x_hal.hpp"
#include "logging/log.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

FakeWire Wire;
namespace {
std::string mode, logs;
std::vector<std::string> scans;
bool tof_attempted = false, imu_stopped = false, motor_stopped = false;
bool start_services() {
  assert(false && "diagnostics must never start localization/services");
  return false;
}
} // namespace
bool hal_motor_init() {
  assert(scans.size() == 4); // Both GPIO and hardware inventories precede HALs.
  return true;
}
void hal_motor_stop() { motor_stopped = true; }
int64_t hal_time_us() { return 1; }
namespace remote {
bool board_init() { return true; }
} // namespace remote
namespace imu {
bool initialize(const Config &) {
  assert(Wire.begin_calls == 1); // Preparing sensors must not reset the bus.
  return mode != "imu";
}
Statistics statistics() {
  Statistics result;
  result.online = true;
  result.magnetometer_online = mode != "mag";
  return result;
}
void stop() { imu_stopped = true; }
} // namespace imu
namespace tof {
bool initialize(const Config &config) {
  assert(config.i2c->Instance == I2C2);
  for (unsigned i = 0; i < 3; ++i)
    assert(config.sensors[i].address_7bit == 0x30 + i);
  tof_attempted = true;
  return mode != "tof";
}
} // namespace tof
void i2c_diagnostics::scan(I2C_HandleTypeDef *, const char *name,
                           const char *phase, const Binding *, std::size_t) {
  scans.push_back(std::string(name) + ":" + phase);
}
void i2c_diagnostics::scan_gpio(int, int, const char *name, const Binding *,
                                std::size_t) {
  assert(Wire.begin_calls == 0); // Neither controller/device has started yet.
  scans.push_back(std::string(name) + ":GPIO");
}
void i2c_diagnostics::state(I2C_HandleTypeDef *, int, int, const char *) {}
robot_log::Level robot_log::minimum_level() { return Level::TRACE; }
bool robot_log::write(Level, const char *, const char *format, ...) {
  char text[128];
  va_list args;
  va_start(args, format);
  std::vsnprintf(text, sizeof text, format, args);
  va_end(args);
  logs += std::string(text) + '\n';
  return true;
}
void boot_sensors::capture(Snapshot &) {
  assert(tof_attempted);
  assert(scans.back() == "ToF:after ToF init attempt");
}
void boot_sensors::print(const Snapshot &) {
  assert(bup::bup_task_get_status() == bup::BupStatus::FAILED);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  mode = argv[1];
  Wire.registers[0x12] = mode == "tcs" ? 0 : 0x44;
  assert(bup::bup_task_start(start_services));
  fake::run("bup_task", [] { assert(false); });
  assert(tof_attempted);
  assert(scans.size() == 6);
  assert(scans[0] == "main:GPIO" && scans[1] == "ToF:GPIO");
  assert(scans[2] == "main:before device HALs");
  assert(scans[3] == "ToF:before device HALs");
  assert(scans.back() == "ToF:after ToF init attempt");
  assert(motor_stopped);
  assert(bup::bup_task_get_status() == bup::BupStatus::FAILED);
  assert(imu_stopped == (mode == "imu" || mode == "mag"));
  if (mode == "imu" || mode == "mag" || mode == "healthy") {
    assert(Wire.registers[1] == 0xF6);
    assert(logs.find("sensor diagnostics complete") != std::string::npos);
  } else {
    assert(logs.find("sensor init failed") != std::string::npos);
  }
}
