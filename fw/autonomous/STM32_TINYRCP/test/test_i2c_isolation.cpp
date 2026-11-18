// SPDX-License-Identifier: GPL-3.0-only
#include "bup/bup_tasks.hpp"
#include "bup/i2c_isolation.hpp"
#include "hal/i2c_diagnostics.hpp"
#include "logging/log.hpp"
#include "support/fake_rtos.hpp"
#include <Wire.h>
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

FakeWire Wire;
namespace {
std::vector<std::string> calls;
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
bool robot_log::write(Level, const char *, const char *, ...) { return true; }
int main(int argc, char **argv) {
  assert(argc == 2);
  const auto mode = unsigned(std::atoi(argv[1]));
  ths_i2c_isolation_case = mode;
  assert(bup::bup_task_start(callback));
  assert(calls.empty()); // Initialization belongs to BUP, not its creator.
  fake::run("bup_task", [] { assert(false); });
  assert(bup::bup_task_get_status() == bup::BupStatus::FAILED);
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
      {"tof_bus", "tof"}};
  assert(calls == (mode < expected.size() ? expected[mode]
                                          : std::vector<std::string>{}));
}
