// SPDX-License-Identifier: GPL-3.0-only
#include "bup/bup_tasks.hpp"
#include "bup/i2c_isolation.hpp"
#include "hal/i2c_diagnostics.hpp"
#include "hal/imu_hal.hpp"
#include "hal/lsm303dlhc_hal.hpp"
#include "hal/vl53l0x_hal.hpp"
#include "logging/log.hpp"
#include "support/fake_rtos.hpp"
#include <Wire.h>
#include <cassert>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

FakeWire Wire;
namespace {
std::vector<std::string> calls;
std::string fault;
int soak_result = -1;
bool callback() {
  assert(false && "isolation must never start services");
  return false;
}
bool record(const char *name) {
  calls.push_back(name);
  return true;
}
} // namespace
bool hal_board_main_bus_init(bool configure_clock) {
  return record(configure_clock ? "main" : "main_no_clock");
}
bool hal_board_tof_bus_init() { return record("tof_bus"); }
bool hal_board_sensor_buses_init() { return record("inventories"); }
bool hal_board_imu_init() { return record("imu"); }
bool hal_board_tof_init() { return record("tof"); }
bool hal_tcs34725_init() { return record("tcs"); }
int64_t hal_time_us() { return fake::now; }
namespace lsm303dlhc {
bool read_acceleration(loc::ImuSample *, int64_t *) { return true; }
}
namespace imu {
bool read_magnetic_native(loc::MagSample *, int64_t *) { return true; }
Statistics statistics() {
  Statistics s;
  s.online = s.magnetometer_online = true;
  s.bus_errors = fault == "imu_fault";
  return s;
}
}
namespace tof {
bool read(loc::TofId, float *, int64_t *) { return true; }
Statistics statistics(loc::TofId) {
  Statistics s;
  s.online = true;
  s.valid = fault == "no_progress" ? 0 : 200;
  s.api_errors = fault == "tof_fault";
  return s;
}
}
bool hal_motor_init() { return record("motor"); }
void hal_motor_stop() { record("stop"); }
namespace remote {
bool board_init() { return record("settings"); }
} // namespace remote
void i2c_diagnostics::state(I2C_HandleTypeDef *, int, int, const char *) {}
void i2c_diagnostics::scan(I2C_HandleTypeDef *, const char *, const char *,
                           const Binding *, std::size_t) {
  record("inventory_after");
}
bool robot_log::write(Level, const char *, const char *format, ...) {
  char message[128];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof message, format, args);
  va_end(args);
  if (std::string(message).find("30s sensor soak=") == 0)
    soak_result = std::string(message).find("=PASS") != std::string::npos;
  return true;
}
int main(int argc, char **argv) {
  assert(argc == 2 || argc == 3);
  if (argc == 3)
    fault = argv[2];
  const auto mode = unsigned(std::atoi(argv[1]));
  ths_i2c_isolation_case = mode;
  assert(bup::bup_task_start(callback));
  assert(calls.empty()); // Initialization belongs to BUP, not its creator.
  fake::run("bup_task", [&] {
    assert(mode == 13);
    fake::now += 2000;
  });
  assert(bup::bup_task_get_status() == bup::BupStatus::FAILED);
  if (mode == 13)
    assert(soak_result == int(fault.empty()));
  const std::vector<std::vector<std::string>> expected = {
      {"main", "inventory_after"},
      {"main", "tcs", "inventory_after"},
      {"main", "imu", "tcs", "inventory_after"},
      {"main", "tof_bus", "tcs", "inventory_after"},
      {"tof_bus", "main", "tcs", "inventory_after"},
      {"main", "tof_bus", "tof", "tcs", "inventory_after"},
      {"motor", "main", "tcs", "inventory_after", "stop"},
      {"main", "settings", "tcs", "inventory_after"},
      {"main", "tof_bus", "motor", "settings", "imu", "tof", "tcs",
       "inventory_after", "stop"},
      {"inventories", "motor", "settings", "imu", "tof", "tcs",
       "inventory_after", "stop"},
      {"main_no_clock", "tcs", "inventory_after"},
      {"main", "imu", "inventory_after"},
      {"tof_bus", "tof"},
      {"inventories", "imu", "tof"}};
  assert(calls == (mode < expected.size() ? expected[mode]
                                          : std::vector<std::string>{}));
}
