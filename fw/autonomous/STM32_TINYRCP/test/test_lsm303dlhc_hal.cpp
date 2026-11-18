// SPDX-License-Identifier: GPL-3.0-only
#include "hal/lsm303dlhc_config.hpp"
#include "hal/lsm303dlhc_hal.hpp"
#include "logging/log.hpp"
#include "lsm303dlhc_device.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
int64_t now = 1000000;
auto &device = lsm_fake::device;
bool close(float a, float b, float tolerance = .011f) {
  return std::fabs(a - b) <= tolerance;
}
imu::Config config() {
  imu::Config c;
  c.i2c = &device.bus;
  return c;
}
void reset() {
  imu::stop();
  device.reset();
  now += 1000000;
  assert(imu::initialize(config()));
}
void frame(float heading = 0, float roll = 0, float pitch = 0) {
  now += 10000;
  const float cr = std::cos(roll), sr = std::sin(roll), cp = std::cos(pitch),
              sp = std::sin(pitch);
  const float x = 45 * std::cos(heading), y = -45 * std::sin(heading), z = 20;
  // Independent inverse rotations synthesize tilted body measurements.
  const float bx = cp * x - sp * z;
  const float by = sp * sr * x + cr * y + cp * sr * z;
  const float bz = sp * cr * x - sr * y + cp * cr * z;
  // Mount native +X left / +Y back / +Z up: native = [body Y, -body X, body Z].
  device.acceleration(9.80665f * cp * sr, 9.80665f * sp, 9.80665f * cp * cr);
  device.magnetic(by, -bx, bz);
}
void raw() {
  reset();
  assert(device.accel[0x20] == 0x57 && device.accel[0x23] == 0x98);
  assert(device.accel[0x22] == 0 && device.accel[0x24] == 0);
  assert(device.mag[0] == 0x1C && device.mag[1] == 0x20 && device.mag[2] == 0);
  device.acceleration(-1.96133f, 0.980665f, 9.80665f);
  device.magnetic(-25, 35, -40);
  loc::ImuSample a{};
  loc::MagSample m{};
  int64_t stamp = -1;
  assert(lsm303dlhc::read_acceleration(&a, &stamp) && stamp == now);
  assert(close(a.ax_mps2, -.980665f) && close(a.ay_mps2, -1.96133f));
  assert(close(a.az_mps2, 9.80665f) && a.gz_rps == 0);
  assert(!lsm303dlhc::read_acceleration(&a, &stamp)); // DRDY consumed.
  assert(imu::read_magnetic_native(&m, &stamp));
  assert(close(m.mx_uT, -25, .05f) && close(m.my_uT, 35, .05f));
  assert(
      close(m.mz_uT, -40, .06f)); // Z has a different gain and byte position.
  assert(!imu::read_magnetic_native(&m, &stamp));
  assert(!imu::read(nullptr, &stamp) && !imu::read(&a, nullptr));
  imu::stop();
  assert(!imu::statistics().online && device.accel[0x20] == 0);
  assert(device.mag[2] == 2 && !imu::read(&a, &stamp));
}
void compass() {
  reset();
  loc::ImuSample a{};
  loc::MagSample m{};
  int64_t stamp = -1;
  frame(3.10f, .35f, -.20f);
  assert(!imu::read(&a, &stamp) && stamp == -1); // Warm-up isn't zero gyro.
  float angle = 3.10f;
  for (unsigned i = 0; i < 100; ++i) {
    angle += .005f; // +0.5 rad/s through the +/-pi boundary, while tilted.
    frame(angle, .35f, -.20f);
    assert(hal_imu_read(&a, &stamp));
    assert(hal_mag_read(&m, &stamp));
    assert(!hal_mag_read(&m, &stamp)); // Pending field delivered once.
    assert(std::fabs(loc::wrap_pi(std::atan2(-m.my_uT, m.mx_uT) - angle)) <
           .006f);
    if (i > 40)
      assert(close(a.gz_rps, .5f, .08f));
    assert(a.gx_rps == 0 && a.gy_rps == 0);
  }
  assert(lsm303dlhc::magnetic_heading_usable());
  now += 60000;
  assert(!lsm303dlhc::magnetic_heading_usable());
  frame(angle);
  assert(!imu::read(&a, &stamp)); // Gap resets derivative history.
  frame(angle);
  assert(imu::read(&a, &stamp));
  frame(angle);
  device.acceleration(0, 0, 15);
  assert(!imu::read(&a, &stamp));
  frame(angle);
  assert(!imu::read(&a, &stamp)); // Invalid tilt also resets history.
  frame(angle);
  assert(imu::read(&a, &stamp));
  frame(angle + 1);
  assert(!imu::read(&a, &stamp)); // Impossible instantaneous heading jump.
}
void faults() {
  reset();
  const unsigned init_transfers = device.transfers;
  // Exercise every upstream identity, callback write, readback and drain.
  for (unsigned i = 1; i <= init_transfers; ++i) {
    imu::stop();
    device.reset();
    device.fail_at = i;
    assert(!imu::initialize(config()));
    assert(!imu::statistics().online);
  }
  reset();
  imu::stop();
  device.reset();
  device.corrupt_readback = true;
  assert(!imu::initialize(config()));
  device.reset();
  device.mag[0x0B] = 0;
  assert(!imu::initialize(config()));
  auto bad = config();
  bad.i2c_timeout_ms = UINT32_MAX;
  assert(!imu::initialize(bad));
  reset();
  loc::MagSample m{1, 2, 3};
  int64_t stamp = -1;
  device.magnetic(30, 20, 10);
  device.mag[9] = 3;
  assert(!imu::read_magnetic_native(&m, &stamp));
  assert(device.mag[9] == 0 && imu::statistics().mag_overruns == 1);
  assert(m.mx_uT == 1 && stamp == -1);
  device.magnetic(400, 20, 10);
  assert(!imu::read_magnetic_native(&m, &stamp));
  assert(imu::statistics().mag_overflows == 1);
  device.magnetic(30, 20, 10);
  device.fail_at =
      device.transfers + 2; // MAG data transfer fails after status.
  assert(!imu::read_magnetic_native(&m, &stamp));
  assert(imu::statistics().bus_errors == 1);
  device.fail_at = 0;
  device.advance_time = [](int64_t dt) { now += dt; };
  device.latency_us = 15000; // Two transfers exceed the read-duration budget.
  assert(!imu::read_magnetic_native(&m, &stamp));
  device.latency_us = 0;
  device.magnetic(30, 20, 10);
  assert(imu::read_magnetic_native(&m, &stamp));
  device.magnetic(30, 20, 10);
  --now;
  assert(!imu::read_magnetic_native(&m, &stamp));
}
void unqualified() {
  reset();
  loc::ImuSample a{1, 2, 3, 4, 5, 6};
  loc::MagSample m{};
  int64_t stamp = -1;
  for (int i = 0; i < 5; ++i) {
    frame();
    assert(!imu::read(&a, &stamp) && !imu::read_magnetic(&m, &stamp));
    assert(a.gz_rps == 6 && stamp == -1);
  }
  assert(imu::statistics().online && !lsm303dlhc::magnetic_heading_usable());
}
} // namespace
int64_t hal_time_us() { return now; }
namespace robot_log {
bool write(Level, const char *, const char *, ...) { return true; }
} // namespace robot_log
int main(int argc, char **argv) {
  assert(argc == 2);
  if (!std::strcmp(argv[1], "raw"))
    raw();
  else if (!std::strcmp(argv[1], "compass"))
    compass();
  else if (!std::strcmp(argv[1], "faults"))
    faults();
  else if (!std::strcmp(argv[1], "unqualified"))
    unqualified();
  else
    assert(false);
  std::puts("PASS LSM303DLHC register processing");
}
