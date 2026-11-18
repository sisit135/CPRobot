// SPDX-License-Identifier: GPL-3.0-only

// Board hooks use the single sensor backend selected at build time.
#include "hal/i2c_diagnostics.hpp"
#include "hal/imu_hal.hpp"
#if AXI_IMU_LSM303DLHC
#include "hal/lsm303dlhc_config.hpp"
#include "hal/lsm303dlhc_hal.hpp"
#endif
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
namespace {
using namespace board::sensors;
const i2c_diagnostics::Binding main_bindings[] = {
#if AXI_IMU_LSM303DLHC
    {board::lsm303dlhc::kAccelAddress7bit, "LSM303DLHC accel"},
    {board::lsm303dlhc::kMagAddress7bit, "LSM303DLHC mag"},
#else
    {kMpu9250Address7bit, "MPU9250 accel/gyro"},
    {kAk8963Address7bit, "AK8963 (needs bypass)"},
#endif
    {kTcs34725Address7bit, "TCS3472 color"}};
const i2c_diagnostics::Binding tof_bindings[] = {
    {kVl53l0xBootAddress7bit, "VL53L0X boot (shared)"},
    {kTofAddress7bit[0], "VL53L0X left (assigned)"},
    {kTofAddress7bit[1], "VL53L0X front (assigned)"},
    {kTofAddress7bit[2], "VL53L0X right (assigned)"}};
bool buses_ready = false, main_ready = false, tof_ready = false;
TwoWire &tof_bus() {
  static TwoWire wire{uint32_t(kTofSdaPin), uint32_t(kTofSclPin)};
  return wire;
}
} // namespace

namespace {
bool valid_bus_wiring() {
  // Reset VL53L0X (0x29) must not share the TCS34725 controller.
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
  return true;
}
} // namespace

bool hal_board_main_bus_init(bool configure_clock) {
  if (main_ready)
    return true;
  Wire.setSDA(kMainSdaPin);
  Wire.setSCL(kMainSclPin);
  Wire.begin();
  if (configure_clock)
    Wire.setClock(kMainI2cHz);
  main_ready = Wire.getHandle() && Wire.getHandle()->Instance;
  if (main_ready && tof_ready &&
      Wire.getHandle()->Instance == tof_bus().getHandle()->Instance)
    main_ready = false;
  i2c_diagnostics::state(Wire.getHandle(), kMainSdaPin, kMainSclPin,
                         "after main Wire.begin");
  return main_ready;
}

bool hal_board_tof_bus_init() {
  if (tof_ready)
    return true;
  if (!valid_bus_wiring())
    return false;
  auto &wire = tof_bus();
  wire.begin();
  wire.setClock(kMainI2cHz);
  tof_ready = wire.getHandle() && wire.getHandle()->Instance;
  if (tof_ready && main_ready &&
      wire.getHandle()->Instance == Wire.getHandle()->Instance)
    tof_ready = false;
  i2c_diagnostics::state(wire.getHandle(), kTofSdaPin, kTofSclPin,
                         "after ToF Wire.begin");
  if (main_ready)
    i2c_diagnostics::state(Wire.getHandle(), kMainSdaPin, kMainSclPin,
                           "after ToF Wire.begin");
  return tof_ready;
}

bool hal_board_sensor_buses_init() {
  if (buses_ready)
    return true;
#if defined(STM32F411xE)
  robot_log::write(robot_log::Level::INFO, "i2c", "reset CSR=%08lx", RCC->CSR);
#endif
  if (!valid_bus_wiring())
    return false;
  // No device HAL, address assignment, XSHUT change or Wire initialization
  // precedes these probes. A stuck first bus must not hide the second bus.
  i2c_diagnostics::scan_gpio(kMainSdaPin, kMainSclPin, "main", main_bindings,
                             3);
  i2c_diagnostics::scan_gpio(kTofSdaPin, kTofSclPin, "ToF", tof_bindings, 4);
  if (!hal_board_main_bus_init())
    return false;
  i2c_diagnostics::scan(Wire.getHandle(), "main", "before device HALs",
                        main_bindings, 3);
  if (!hal_board_tof_bus_init())
    return false;
  robot_log::write(robot_log::Level::INFO, "tof", "bus ready");
  // Scan both buses before the first device failure can terminate bring-up.
  // Multiple unassigned ToFs can ACK 0x29; this cannot identify/count them.
  i2c_diagnostics::scan(tof_bus().getHandle(), "ToF", "before device HALs",
                        tof_bindings, 4);
  buses_ready = true;
  return true;
}

bool hal_board_imu_init() {
  imu::Config config;
  config.i2c = Wire.getHandle(); // Prepared once by BUP, never restarted here.
  robot_log::write(robot_log::Level::INFO, "imu", "init started");
  bool imu_ready = imu::initialize(config);
  i2c_diagnostics::state(Wire.getHandle(), kMainSdaPin, kMainSclPin,
                         "after IMU init attempt");
  if (!imu_ready) {
    robot_log::write(robot_log::Level::ERROR, "imu", "health check failed");
  } else if (!imu::statistics().online ||
             !imu::statistics().magnetometer_online) {
    robot_log::write(robot_log::Level::ERROR, "imu",
                     "motion sensor/magnetometer health check failed");
    imu::stop();
    imu_ready = false;
  }
  if (imu_ready)
    robot_log::write(robot_log::Level::INFO, "imu", "hw ready");
  return imu_ready;
}

bool hal_board_tof_init() {
  tof::Config ranges;
  ranges.i2c = tof_bus().getHandle();
  for (unsigned i = 0; i < 3; ++i) {
    ranges.sensors[i].xshut_pin = kTofXshutPin[i];
    ranges.sensors[i].address_7bit = kTofAddress7bit[i];
  }
  const bool tof_ready = tof::initialize(ranges);
  i2c_diagnostics::state(tof_bus().getHandle(), kTofSdaPin, kTofSclPin,
                         "after ToF init attempt");
  if (!tof_ready) {
    robot_log::write(robot_log::Level::ERROR, "tof", "health check failed");
    return false;
  }
  robot_log::write(robot_log::Level::INFO, "tof", "hw ready");
  return true;
}
bool hal_board_sensors_init() {
  if (!hal_board_sensor_buses_init())
    return false;
  i2c_diagnostics::state(Wire.getHandle(), kMainSdaPin, kMainSclPin,
                         "after motor/settings HALs");
  i2c_diagnostics::state(tof_bus().getHandle(), kTofSdaPin, kTofSclPin,
                         "after motor/settings HALs");
  if (!hal_board_imu_init()) {
#if AXI_BUP_IMU_DIAGNOSTIC
    imu::stop();
    robot_log::write(
        robot_log::Level::WARN, "bup",
        "IMU failure deferred for diagnostics; localization blocked");
#else
    return false;
#endif
  }
  i2c_diagnostics::scan(Wire.getHandle(), "main", "after IMU init attempt",
                        main_bindings, 3);
  const bool ready = hal_board_tof_init();
  i2c_diagnostics::scan(tof_bus().getHandle(), "ToF", "after ToF init attempt",
                        tof_bindings, 4);
  return ready;
}
bool hal_magnetic_clean() {
#if AXI_IMU_LSM303DLHC
  return lsm303dlhc::magnetic_heading_usable();
#else
  return false;
#endif
}
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
