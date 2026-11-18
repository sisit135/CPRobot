// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "hal/imu_selection.hpp"
#include "localization/types.hpp"
#include "stm32f4xx_hal.h"
#include <cstdint>

namespace imu {
constexpr bool kHasGyroscope = AXI_IMU_MPU9250;
enum class Event : uint8_t {
  INIT,
  READY,
  API_ERROR,
  REJECTED,
  STOP,
  BAD_CONFIG
};
struct LogRecord {
  int64_t timestamp_us = 0;
  Event event = Event::INIT;
  int code = 0;
  const char *message = nullptr; // Valid only during the LogSink call.
};
using LogSink = void (*)(const LogRecord &record, void *context);
// A custom sink must copy/enqueue the record and message immediately, without
// blocking or re-entering this HAL. The message pointer is not persistent.
struct Config {
  I2C_HandleTypeDef *i2c =
      nullptr; // Initialized Wire.getHandle(), never owned.
  uint32_t i2c_timeout_ms = 5;
  uint32_t max_read_duration_us = 20000;
  uint32_t error_log_period_us = 1000000;
  LogSink log = nullptr; // nullptr: deferred robot_log pipeline (source: imu).
  void *log_context = nullptr;
};
struct Statistics {
  uint32_t imu_samples = 0, mag_samples = 0, not_ready = 0;
  uint32_t rejected = 0, bus_errors = 0, log_dropped = 0;
  uint32_t mag_overruns = 0, mag_overflows = 0;
  uint32_t last_hal_error = 0;
  bool online = false, magnetometer_online = false;
};
// Sensor owner only; the caller starts and retains ownership of the I2C bus.
// MPU9250: 100 Hz accel/gyro (+/-4 g, +/-500 deg/s), optional AK8963.
// LSM303DLHC: ST BSP initialization, 100 Hz +/-4 g high-resolution
// acceleration, 220 Hz +/-1.3 gauss magnetometer. Both identities/readbacks
// must pass. A true result means hardware initialized; calibration may still
// gate data.
bool initialize(const Config &config);

// Fresh body acceleration (m/s^2, including gravity) and planar yaw rate
// (rad/s). MPU9250 supplies measured gyro channels. LSM303DLHC supplies a
// filtered, wrapped compass-heading derivative in gz_rps; gx_rps/gy_rps are
// placeholders. The LSM backend requires two valid headings, verified
// calibration and a verified magnetic environment during motion. It ignores MPU
// gyro-zero tuning. Synchronous bounded HAL transfers, no conversion wait.
// Read-completion times share hal_time_us(); false leaves caller outputs
// unchanged.
bool read(loc::ImuSample *out, int64_t *timestamp_us);

// Fresh native field in microteslas for diagnostics/calibration. MPU applies
// factory ASA adjustment. LSM reorders wire X,Z,Y and uses separate XY/Z gains.
// Consumes the same hardware magnetic stream as read()/read_magnetic().
bool read_magnetic_native(loc::MagSample *out, int64_t *timestamp_us);

// Calibrated, gravity-levelled body field, microteslas; false before
// qualification. MPU uses fresh quasi-static tilt and a separate magnetic-clean
// board gate. LSM uses ACCEL tilt; call read() first, then read_magnetic()
// consumes its pending field once. Dynamic acceleration can invalidate the tilt
// estimate.
bool read_magnetic(loc::MagSample *out, int64_t *timestamp_us);

// Power down without closing the shared bus; subsequent reads return false.
void stop();
// No bus access. Synchronize externally if observing from another task.
Statistics statistics();
} // namespace imu

bool hal_imu_read(loc::ImuSample *out, int64_t *timestamp_us);
bool hal_mag_read(loc::MagSample *out, int64_t *timestamp_us);
