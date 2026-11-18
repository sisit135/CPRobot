// SPDX-License-Identifier: GPL-3.0-only
#include "bup/i2c_isolation.hpp"
#if AXI_BUP_I2C_ISOLATION
#include "hal/i2c_diagnostics.hpp"
#include "hal/imu_hal.hpp"
#include "hal/lsm303dlhc_config.hpp"
#include "hal/sensors_config.hpp"
#include "localization/hal_sensors.hpp"
#include "logging/log.hpp"
#include "motor_control/hal_motor.hpp"
#include "remote/board_store.hpp"
#include <Wire.h>

extern "C" {
volatile uint32_t ths_i2c_isolation_case = 1;
}
namespace bup {
static_assert(AXI_IMU_LSM303DLHC,
              "I2C isolation uses the LSM303DLHC bench wiring");
void run_i2c_isolation() {
  using namespace board::sensors;
  static const char *const names[] = {"main controller only",
                                      "TCS only",
                                      "IMU then TCS",
                                      "main then ToF controller then TCS",
                                      "ToF controller then main then TCS",
                                      "ToF devices then TCS",
                                      "motor then TCS",
                                      "settings then TCS",
                                      "normal order without inventories",
                                      "normal order with inventories",
                                      "TCS without setClock",
                                      "IMU only",
                                      "ToF only"};
  const auto selected = ths_i2c_isolation_case;
  if (selected >= sizeof(names) / sizeof(names[0])) {
    robot_log::write(robot_log::Level::ERROR, "i2c.iso", "invalid case=%lu",
                     static_cast<unsigned long>(selected));
    return;
  }
  robot_log::write(robot_log::Level::INFO, "i2c.iso", "case=%lu %s",
                   static_cast<unsigned long>(selected), names[selected]);
  const auto step = [](const char *name, bool result) {
    robot_log::write(robot_log::Level::INFO, "i2c.iso", "%s=%s", name,
                     result ? "PASS" : "FAIL");
    return result;
  };
  if (selected == 12) {
    if (step("ToF controller", hal_board_tof_bus_init()))
      step("ToF devices", hal_board_tof_init());
    robot_log::write(robot_log::Level::INFO, "i2c.iso",
                     "case=12 complete; services blocked");
    return;
  }
  const bool normal = selected == 8 || selected == 9;
  bool motor_owned = false;
  if (selected == 6) {
    motor_owned = true;
    step("motor", hal_motor_init());
  }
  if (selected == 4)
    step("ToF controller", hal_board_tof_bus_init());
  const bool main =
      selected == 9
          ? step("inventories", hal_board_sensor_buses_init())
          : step("main controller", hal_board_main_bus_init(selected != 10));
  if (main) {
    if (selected == 3 || selected == 5 || selected == 8)
      step("ToF controller", hal_board_tof_bus_init());
    if (normal) {
      motor_owned = true;
      step("motor", hal_motor_init());
    }
    if (selected == 7 || normal)
      step("settings", remote::board_init());
    if (selected == 2 || selected == 11 || normal)
      step("IMU", hal_board_imu_init());
    if (selected == 5 || normal)
      step("ToF devices", hal_board_tof_init());
    // No inventory precedes the first TCS transaction except in case 9.
    if (selected != 0 && selected != 11)
      step("TCS", hal_tcs34725_init());
    i2c_diagnostics::state(Wire.getHandle(), kMainSdaPin, kMainSclPin,
                           "isolation result");
    // Run this after the isolated experiment so probing cannot cause its
    // result.
    const i2c_diagnostics::Binding bindings[] = {
        {board::lsm303dlhc::kAccelAddress7bit, "LSM303DLHC accel"},
        {board::lsm303dlhc::kMagAddress7bit, "LSM303DLHC mag"},
        {kTcs34725Address7bit, "TCS3472 color"}};
    i2c_diagnostics::scan(Wire.getHandle(), "main", "after isolated init",
                          bindings, 3);
  }
  if (motor_owned)
    hal_motor_stop();
  robot_log::write(robot_log::Level::INFO, "i2c.iso",
                   "case=%lu complete; services blocked",
                   static_cast<unsigned long>(selected));
}
} // namespace bup
#endif
