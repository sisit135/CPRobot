// SPDX-License-Identifier: GPL-3.0-only

#include "localization/hal_sensors.hpp"
#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "platform/hal_platform.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
unsigned sensor_trace_records = 0;
unsigned sensor_reads = 0;
unsigned tof_reads = 0;
std::string sensor_trace_format;
} // namespace

int64_t hal_time_us() { return fake::now; }
void hal_log_pose(const loc::PoseSnapshot &) {}
bool hal_imu_read(loc::ImuSample *out, int64_t *timestamp_us) {
  *out = {1, 2, 9.8f, .1f, .2f, .3f};
  *timestamp_us = fake::now;
  ++sensor_reads;
  return true;
}
bool hal_mag_read(loc::MagSample *out, int64_t *timestamp_us) {
  *out = {40, 2, -3};
  *timestamp_us = fake::now;
  return true;
}
bool hal_magnetic_clean() { return false; }
bool hal_tof_red_distance_corrected(loc::TofId id, float *distance_m,
                                  int64_t *timestamp_us) {
  *distance_m = .2f + .1f * static_cast<unsigned>(id);
  *timestamp_us = fake::now;
  ++tof_reads;
  return true;
}
bool hal_tcs34725_read(loc::RgbcSample *, int64_t *) {
  assert(false && "disabled TCS path must never be polled");
  return false;
}
bool hal_end_tape_classify(const loc::RgbcSample &, int64_t,
                           nav::EndTapeEvent *) {
  return false;
}

namespace robot_log {
bool write(Level level, const char *source, const char *format, ...) {
  if (level == Level::TRACE && std::strcmp(source, "sensors") == 0) {
    ++sensor_trace_records;
    sensor_trace_format = format;
  }
  return true;
}
} // namespace robot_log

int main() {
  assert(loc::localization_sensors_start());
  unsigned updates = 0;
  fake::run("loc_sensor", [&] {
    ++updates;
    fake::now += 10000;
    if (updates == 15)
      fake::finish();
  });
  assert(updates == 15);
  assert(sensor_reads == updates);
  assert(tof_reads >= 6); // Three ToF channels on each 50 ms poll.
  assert(sensor_trace_records == updates / 5);
  assert(sensor_trace_format.find("imu=") != std::string::npos &&
         sensor_trace_format.find("gz=%.3f") != std::string::npos &&
         sensor_trace_format.find("mag=") != std::string::npos &&
         sensor_trace_format.find("tof=%.3f,%.3f,%.3f") != std::string::npos);
  std::puts("PASS sensor task: one TRACE reading per five 100 Hz updates");
}
