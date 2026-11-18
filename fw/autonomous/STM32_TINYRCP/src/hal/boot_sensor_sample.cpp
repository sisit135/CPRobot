// SPDX-License-Identifier: GPL-3.0-only
#include "hal/boot_sensor_sample.hpp"
#include "hal/imu_hal.hpp"
#if AXI_IMU_LSM303DLHC
#include "hal/lsm303dlhc_hal.hpp"
#endif
#include "hal/vl53l0x_hal.hpp"
#include "localization/hal_sensors.hpp"
#include "logging/log.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include <cstdio>

namespace boot_sensors {
void capture(Snapshot &out) {
  out = {};
  bool imu_done = !imu::statistics().online;
  bool mag_done = !imu::statistics().magnetometer_online;
  bool color_done = !hal_tcs34725_initialized();
  bool range_done[3]{};
  const int64_t deadline = hal_time_us() + 500000;
  while (hal_time_us() < deadline) {
    int64_t stamp = 0;
    if (!imu_done) {
#if AXI_IMU_LSM303DLHC
      out.imu_valid = lsm303dlhc::read_acceleration(&out.imu, &stamp);
#else
      out.imu_valid = imu::read(&out.imu, &stamp);
#endif
      imu_done = out.imu_valid || !imu::statistics().online;
    }
    if (!mag_done) {
      // Native factory-adjusted field, for diagnostics only; never EKF input.
      out.mag_valid = imu::read_magnetic_native(&out.mag, &stamp);
      mag_done = out.mag_valid || !imu::statistics().magnetometer_online;
    }
    if (!color_done) {
      out.color_valid = hal_tcs34725_read(&out.color, &stamp);
      color_done = out.color_valid || !hal_tcs34725_initialized();
    }
    for (unsigned i = 0; i < 3; ++i) {
      if (range_done[i])
        continue;
      const auto id = static_cast<loc::TofId>(i);
      if (!tof::statistics(id).online) {
        range_done[i] = true;
        continue;
      }
      if (tof::read(id, &out.range_m[i], &stamp)) {
        out.range_state[i] = RangeState::VALID;
        range_done[i] = true;
      } else {
        const auto stats = tof::statistics(id);
        if (!stats.online) {
          range_done[i] = true;
        } else if (stats.rejected) {
          if (stats.last_out_of_range)
            out.range_state[i] = RangeState::OUT_OF_RANGE;
          range_done[i] = true;
        }
      }
    }
    if (imu_done && mag_done && color_done && range_done[0] && range_done[1] &&
        range_done[2])
      break;
    vTaskDelay(pdMS_TO_TICKS(2) ? pdMS_TO_TICKS(2) : 1);
  }
}

namespace {
void row(const char *sensor, const char *value) {
  robot_log::write(robot_log::Level::INFO, "bup.sample", "| %-14s | %-58s |",
                   sensor, value);
  vTaskDelay(1); // Allow the deferred logger to drain between rows.
}
void vector_row(const char *sensor, bool valid, float x, float y, float z,
                const char *units) {
  char value[59];
  if (valid)
    std::snprintf(value, sizeof value, "x=%.3f y=%.3f z=%.3f %s", double(x),
                  double(y), double(z), units);
  else
    std::snprintf(value, sizeof value, "N/A");
  row(sensor, value);
}
} // namespace
void print(const Snapshot &sample) {
  robot_log::write(robot_log::Level::INFO, "bup.sample",
                   "Sensor snapshot (sampled before service startup)");
  robot_log::write(robot_log::Level::INFO, "bup.sample",
                   "+----------------+-----------------------------------------"
                   "-------------------+");
  robot_log::write(robot_log::Level::INFO, "bup.sample",
                   "| Sensor         | Reading                                 "
                   "                   |");
  vector_row("Acceleration", sample.imu_valid, sample.imu.ax_mps2,
             sample.imu.ay_mps2, sample.imu.az_mps2, "m/s^2");
  vector_row("Gyroscope", sample.imu_valid && imu::kHasGyroscope, sample.imu.gx_rps,
             sample.imu.gy_rps, sample.imu.gz_rps, "rad/s");
  vector_row("Mag native", sample.mag_valid, sample.mag.mx_uT, sample.mag.my_uT,
             sample.mag.mz_uT, "uT");
  char value[59];
  if (sample.color_valid)
    std::snprintf(value, sizeof value, "R=%u G=%u B=%u C=%u%s",
                  unsigned(sample.color.red), unsigned(sample.color.green),
                  unsigned(sample.color.blue), unsigned(sample.color.clear),
                  sample.color.saturated ? " saturated" : "");
  else
    std::snprintf(value, sizeof value, "N/A");
  row("Color", value);
  const char *names[] = {"ToF left", "ToF front", "ToF right"};
  for (unsigned i = 0; i < 3; ++i) {
    if (sample.range_state[i] == RangeState::VALID)
      std::snprintf(value, sizeof value, "%.3f m", double(sample.range_m[i]));
    else
      std::snprintf(value, sizeof value, "%s",
                    sample.range_state[i] == RangeState::OUT_OF_RANGE ? "Inf"
                                                                      : "N/A");
    row(names[i], value);
  }
  robot_log::write(robot_log::Level::INFO, "bup.sample",
                   "+----------------+-----------------------------------------"
                   "-------------------+");
}
} // namespace boot_sensors
