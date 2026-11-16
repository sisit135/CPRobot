// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "localization/types.hpp"
#include "stm32f4xx_hal.h"
#include <cstdint>

namespace imu {
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
  LogSink log = nullptr; // nullptr: best-effort Serial, without Serial.begin().
  void *log_context = nullptr;
};
struct Statistics {
  uint32_t imu_samples = 0, mag_samples = 0, not_ready = 0;
  uint32_t rejected = 0, bus_errors = 0, log_dropped = 0;
  uint32_t mag_overruns = 0, mag_overflows = 0;
  uint32_t last_hal_error = 0;
  bool online = false, magnetometer_online = false;
};
/** @brief Initialize MPU9250 accel/gyro and try to initialize its AK8963.
 * @param[in] config Persistent, initialized I2C handle and optional log sink.
 * @returns true when accel/gyro are ready; a missing AK8963 is logged and is
 * reflected in statistics().magnetometer_online, without disabling the gyro.
 * @note Uses libdriver/mpu9250, polling HAL I2C, 100 Hz, +/-4 g, +/-500 deg/s.
 * FIFO, DMP and all MPU interrupt sources are disabled. Startup may take about
 * two seconds on reset failure. Use one startup/sensor task; not ISR-safe.
 * Main sensor pins, addresses, mounting and calibration: sensors_config.hpp.
 */
bool initialize(const Config &config);
/** @brief Poll and consume one fresh accel/gyro frame in body axes.
 * @param[out] out Non-null output, accel m/s^2 (including gravity), gyro rad/s.
 * @param[out] timestamp_us Non-null shared hal_time_us() read-completion time.
 * @returns true once per fresh valid frame. Outputs are unchanged on false.
 * @note Calls use synchronous HAL transfers with finite timeout parameters;
 * internal HAL busy waits can add latency. This is not a hard WCET guarantee.
 * There is no integration wait.
 * No timestamp is inferred for the sensor's sampling instant or filter delay.
 */
bool read(loc::ImuSample *out, int64_t *timestamp_us);
/** @brief Read one fresh factory-adjusted AK8963 sample for
 * calibration/logging.
 * @param[out] out Non-null destination in native AK8963 axes, microteslas.
 * @param[out] timestamp_us Non-null shared read-completion timestamp.
 * @returns true for new data with DRDY=1, DOR=0, HOFL=0 and 16-bit mode.
 * @note Consumes the same stream as read_magnetic(); call only one per cycle.
 */
bool read_magnetic_native(loc::MagSample *out, int64_t *timestamp_us);
/** @brief Read calibrated, gravity-levelled body field for the existing EKF.
 * @param[out] out Non-null output, levelled +X forward/+Y left/+Z up,
 * microteslas.
 * @param[out] timestamp_us Non-null shared read-completion timestamp.
 * @returns true only after explicit calibration/axis verification and a fresh
 * quasi-static accelerometer tilt estimate. Outputs are unchanged on false.
 * @note This is a quasi-static tilt estimate, not a dynamic attitude filter.
 * The existing hal_magnetic_clean() board hook must separately gate motor
 * magnetic interference; this HAL never infers cleanliness from motor demand.
 */
bool read_magnetic(loc::MagSample *out, int64_t *timestamp_us);
/** @brief Enter power-down/sleep without closing the shared I2C bus.
 * @returns No value. Use the sole sensor owner; subsequent reads return false.
 */
void stop();
/** @brief Get diagnostics without accessing the bus.
 * @returns Counters since initialize(); synchronize externally across tasks.
 */
Statistics statistics();
} // namespace imu

/** @brief Existing EKF sensor-task adapter for imu::read().
 * @param[out] out Accel in m/s^2 and gyro in rad/s, body frame.
 * @param[out] timestamp_us Shared monotonic read-completion timestamp.
 * @returns true only for a fresh valid frame.
 */
bool hal_imu_read(loc::ImuSample *out, int64_t *timestamp_us);
/** @brief Existing EKF sensor-task adapter for imu::read_magnetic().
 * @param[out] out Calibrated, gravity-levelled body field in microteslas.
 * @param[out] timestamp_us Shared monotonic read-completion timestamp.
 * @returns true only for a fresh usable frame; false before calibration.
 */
bool hal_mag_read(loc::MagSample *out, int64_t *timestamp_us);
