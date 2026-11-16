// SPDX-License-Identifier: GPL-3.0-only

// Board hooks: MPU9250 implementation is provided by hal/mpu9250_hal.cpp.
#include "hal/mpu9250_hal.hpp"
#include "hal/sensors_config.hpp"
#include "hal/vl53l0x_hal.hpp"
#include "localization/hal_sensors.hpp"
#include "localization/types.hpp"
#include "logging/log.hpp"
#include "motor_control/hal_motor.hpp"
#include "navigation/navigation.hpp"
#include "platform/hal_platform.hpp"
#include <Wire.h>
#ifdef AXI_TELEMETRY_SPI_ENABLE
#include "telemetry/adapter.hpp"
#endif
bool hal_board_sensors_init() {
  imu::Config config;
  config.i2c = Wire.getHandle(); // Wire already started by hal_sensors_init().
  robot_log::write(robot_log::Level::INFO, "imu", "init started");
  if (!imu::initialize(config)) {
    robot_log::write(robot_log::Level::ERROR, "imu", "health check failed");
    return false;
  }
  if (!imu::statistics().online || !imu::statistics().magnetometer_online) {
    robot_log::write(robot_log::Level::ERROR, "imu",
                     "gyro/magnetometer health check failed");
    imu::stop();
    return false;
  }
  robot_log::write(robot_log::Level::INFO, "imu", "hw ready");
  // Reset VL53L0X (0x29) must not share the TCS34725 controller.
  using namespace board::sensors;
  if (kTofSdaPin < 0 || kTofSclPin < 0 || kTofXshutPin[0] < 0 ||
      kTofXshutPin[1] < 0 || kTofXshutPin[2] < 0 || kTofSdaPin == kTofSclPin ||
      uint32_t(kTofSdaPin) == kMainSdaPin ||
      uint32_t(kTofSdaPin) == kMainSclPin ||
      uint32_t(kTofSclPin) == kMainSdaPin ||
      uint32_t(kTofSclPin) == kMainSclPin) {
    robot_log::write(robot_log::Level::ERROR, "tof",
                     "invalid isolated bus wiring");
    return false; // Require real board wiring rather than silently disabling
                  // ranges.
  }
  static TwoWire tof_wire{uint32_t(kTofSdaPin), uint32_t(kTofSclPin)};
  tof_wire.begin();
  tof_wire.setClock(kMainI2cHz);
  if (!tof_wire.getHandle() || !Wire.getHandle() ||
      tof_wire.getHandle()->Instance == Wire.getHandle()->Instance) {
    robot_log::write(robot_log::Level::ERROR, "tof",
                     "isolated bus unavailable");
    return false;
  }
  robot_log::write(robot_log::Level::INFO, "tof", "bus ready");
  tof::Config ranges;
  ranges.i2c = tof_wire.getHandle();
  for (unsigned i = 0; i < 3; ++i) {
    ranges.sensors[i].xshut_pin = kTofXshutPin[i];
    ranges.sensors[i].address_7bit = kTofAddress7bit[i];
  }
  if (!tof::initialize(ranges)) {
    robot_log::write(robot_log::Level::ERROR, "tof", "health check failed");
    return false;
  }
  robot_log::write(robot_log::Level::INFO, "tof", "hw ready");
  return true;
}
bool hal_magnetic_clean() { return false; }
const loc::ColorReference *hal_color_references(std::size_t *count) {
  if (count)
    *count = 0;
  return nullptr; // All-white field: no tile type, quadrant, or mapped color
                  // IDs.
}
void hal_log_pose(const loc::PoseSnapshot &pose) {
#ifdef AXI_TELEMETRY_SPI_ENABLE
  telemetry::submit(pose);
#else
  (void)pose;
#endif
}

void hal_navigation_config(nav::ResolverConfig *) {
} // No screenshot-based calibration.
void hal_navigation_resolved(const loc::MazeMap &) {}
