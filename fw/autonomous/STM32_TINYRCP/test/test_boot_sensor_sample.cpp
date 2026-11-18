// SPDX-License-Identifier: GPL-3.0-only
#include "hal/boot_sensor_sample.hpp"
#include "hal/mpu9250_hal.hpp"
#include "hal/vl53l0x_hal.hpp"
#include "localization/hal_sensors.hpp"
#include "logging/log.hpp"
#include "platform/rtos.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace {
int64_t clock_us = 1;
std::string mode, output;
unsigned imu_reads = 0, mag_reads = 0, color_reads = 0, tof_reads = 0;
bool failed() { return mode == "failed"; }
} // namespace
int64_t hal_time_us() { return clock_us; }
void vTaskDelay(TickType_t ticks) { clock_us += ticks * 1000; }
namespace imu {
Statistics statistics() {
  Statistics result;
  result.online = result.magnetometer_online = !failed();
  return result;
}
bool read(loc::ImuSample *out, int64_t *) {
  ++imu_reads;
  if (mode == "pending")
    return false;
  out->az_mps2 = 9.807f;
  out->gz_rps = .125f;
  return true;
}
bool read_magnetic_native(loc::MagSample *out, int64_t *) {
  ++mag_reads;
  if (mode == "pending")
    return false;
  out->mx_uT = 42;
  return true;
}
} // namespace imu
#if AXI_IMU_LSM303DLHC
namespace lsm303dlhc {
bool read_acceleration(loc::ImuSample *out, int64_t *stamp) {
  const bool valid = imu::read(out, stamp);
  out->gx_rps = out->gy_rps = out->gz_rps = 0;
  return valid;
}
} // namespace lsm303dlhc
#endif
bool hal_tcs34725_initialized() { return !failed(); }
bool hal_tcs34725_read(loc::RgbcSample *out, int64_t *) {
  ++color_reads;
  if (mode == "pending" || clock_us < 30000)
    return false;
  out->red = 123;
  out->clear = 456;
  return true;
}
namespace tof {
Statistics statistics(loc::TofId id) {
  Statistics result;
  result.online = !failed();
  if (mode == "mixed" && id == loc::TofId::FRONT && tof_reads) {
    result.rejected = 1;
    result.last_out_of_range = true;
  }
  if (mode == "mixed" && id == loc::TofId::RIGHT && tof_reads)
    result.online = false;
  return result;
}
bool read(loc::TofId id, float *value, int64_t *) {
  ++tof_reads;
  if (mode == "pending" || (mode == "mixed" && id != loc::TofId::LEFT))
    return false;
  *value = .375f;
  return true;
}
} // namespace tof
bool robot_log::write(Level level, const char *source, const char *format,
                      ...) {
  assert(level == Level::INFO && std::string(source) == "bup.sample");
  char line[96];
  va_list args;
  va_start(args, format);
  const int n = std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  assert(n > 0 && n < int(sizeof line));
  output += std::string(line) + '\n';
  return true;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  mode = argv[1];
  boot_sensors::Snapshot snapshot;
  boot_sensors::capture(snapshot);
  if (failed()) {
    assert(clock_us == 1);
    assert(!imu_reads && !mag_reads && !color_reads && !tof_reads);
  } else if (mode == "pending") {
    assert(clock_us >= 500001 && clock_us <= 502001);
    assert(!snapshot.imu_valid && !snapshot.mag_valid && !snapshot.color_valid);
  } else {
    assert(snapshot.imu_valid && snapshot.mag_valid && !snapshot.color_valid);
    assert(color_reads == 0); // Disabled HAL path is never polled.
    assert(snapshot.range_state[0] == boot_sensors::RangeState::VALID);
    if (mode == "mixed") {
      assert(snapshot.range_state[1] == boot_sensors::RangeState::OUT_OF_RANGE);
      assert(snapshot.range_state[2] == boot_sensors::RangeState::UNAVAILABLE);
    }
  }
  const auto before = imu_reads + mag_reads + color_reads + tof_reads;
  boot_sensors::print(snapshot);
  assert(before == imu_reads + mag_reads + color_reads + tof_reads);
  assert(output.find("ToF left") != std::string::npos);
#if AXI_IMU_LSM303DLHC
  const auto gyro = output.substr(output.find("Gyroscope"), 80);
  assert(gyro.find("N/A") != std::string::npos);
  assert(gyro.find("rad/s") == std::string::npos);
#endif
  assert((output.find("Inf") != std::string::npos) == (mode == "mixed"));
  if (mode == "healthy" || mode == "mixed") {
    assert(output.find("z=9.807") != std::string::npos);
    assert(output.find("R=123") == std::string::npos);
    assert(output.find("0.375 m") != std::string::npos);
  } else {
    assert(output.find("N/A") != std::string::npos);
    assert(output.find("0.000 m") == std::string::npos);
  }
}
