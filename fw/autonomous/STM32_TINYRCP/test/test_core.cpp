#include "localization/color_tracker.hpp"
#include "localization/config.hpp"
#include "localization/fusion_engine.hpp"
#include "localization/robot_ekf.hpp"
#include "motor_control/config.hpp"
#include "motor_control/wheel_controller.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace {
/** @brief Assert proximity for a deterministic numerical test.
 * @param[in] actual Observed value.
 * @param[in] expected Expected value.
 * @param[in] tolerance Maximum absolute error.
 * @returns No value; aborts if the assertion fails.
 */
void near(float actual, float expected, float tolerance) {
  if (std::fabs(actual - expected) > tolerance) {
    std::fprintf(stderr, "near failed: actual=%g expected=%g tolerance=%g\n",
                 actual, expected, tolerance);
    assert(false);
  }
}
/** @brief Check finite, symmetric, positive-semidefinite EKF covariance.
 * @param[in] f Filter under test.
 * @returns No value; aborts on invalid covariance.
 */
void check_covariance(const loc::RobotEKF &f) {
  double L[loc::RobotEKF::N][loc::RobotEKF::N]{};
  for (int i = 0; i < loc::RobotEKF::N; ++i) {
    assert(std::isfinite(f.state(i)));
    for (int j = 0; j <= i; ++j) {
      const float entry = f.covariance(i, j);
      assert(std::isfinite(entry));
      near(entry, f.covariance(j, i), 1e-6f);
      double value = entry;
      for (int k = 0; k < j; ++k)
        value -= L[i][k] * L[j][k];
      if (i == j) {
        assert(value > -1e-8);
        L[i][j] = std::sqrt(std::fmax(value, 1e-14));
      } else
        L[i][j] = value / L[j][j];
    }
  }
}
/** @brief Exercise geometry, angular wrapping, and marker semantics.
 * @returns No value; aborts on regression.
 */
void test_measurements() {
  using namespace loc;
  near(RobotEKF::expected_range(0.5f, 0.5f, 0, cfg::lidar_mounts[1]), 0.23f,
       1e-6f);
  near(RobotEKF::expected_range(0.5f, 0.5f, 0, cfg::lidar_mounts[0]), 0.04f,
       1e-6f);
  assert(std::isnan(
      RobotEKF::expected_range(-0.2f, 0.5f, 0, cfg::lidar_mounts[1])));
  RobotEKF f;
  f.reset(0.5f, 0.5f, kPi / 4);
  assert(!f.update_lidar(std::sqrt(0.5f), {0, 0, 0})); // Corner ambiguity.
  assert(!f.update_lidar(std::numeric_limits<float>::quiet_NaN(),
                         cfg::lidar_mounts[1]));
  f.reset(0.45f, 0.65f, 0);
  const float variance = f.covariance(RobotEKF::X, RobotEKF::X);
  for (int i = 0; i < 100; ++i)
    assert(f.update_marker({MarkerGeometry::CELL, 1, true, 2, 3, 1}));
  near(f.state(RobotEKF::X), 0.45f, 1e-7f);
  near(f.covariance(RobotEKF::X, RobotEKF::X), variance, 1e-7f);
  assert(!f.update_marker({MarkerGeometry::CELL, 1, true, 2, 3, 0}));
  assert(!f.update_marker({MarkerGeometry::CELL, 1, true, 5, 0, 1}));
  f.reset(0.30f, 0.65f, 0);
  assert(f.update_marker({MarkerGeometry::CELL, 1, true, 2, 3, 1}));
  assert(f.state(RobotEKF::X) > 0.40f);
  f.reset(0.5f, 0.5f, deg2rad(179));
  const float angle = deg2rad(-179);
  assert(f.update_mag({45 * std::cos(angle), -45 * std::sin(angle), 0}));
  assert(std::fabs(wrap_pi(f.state(RobotEKF::YAW) - angle)) < deg2rad(3));
  f.reset(-0.05f, 0.5f, 0);
  assert(f.pose().grid_x == -1 && f.pose().grid_y == -1);
  assert(!f.predict({}, 0));
  assert(!f.predict({}, 0.5f));
  ImuSample bad{};
  bad.gz_rps = std::numeric_limits<float>::infinity();
  assert(!f.predict(bad, 0.01f));
  std::puts(
      "PASS measurement geometry, marker membership, angles, invalid input");
}
/** @brief Verify the four-state hold model and one-dimensional event semantics.
 * @returns No value; aborts on an estimator regression.
 */
void test_event_model() {
  using namespace loc;
  static_assert(RobotEKF::N == 4, "No velocity states");
  RobotEKF f, split;
  const float p0 = f.covariance(0, 0);
  ImuSample imu{};
  imu.ax_mps2 = 100;
  imu.ay_mps2 = -100;
  imu.gz_rps = 0.5f;
  assert(f.predict(imu, 0.02f));
  assert(split.predict(imu, 0.01f));
  assert(split.predict(imu, 0.01f));
  near(f.state(0), 0.5f, 1e-7f);
  near(f.state(1), 0.5f, 1e-7f);
  near(f.state(2), 0.01f, 1e-7f);
  assert(f.covariance(0, 0) > p0);
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      near(f.covariance(i, j), split.covariance(i, j), 1e-6f);
  f.reset(0.55f, 0.5f, 0);
  MarkerSample stripe{MarkerGeometry::X_LINE, 1, true, 3, -1, 1};
  const float py = f.covariance(1, 1);
  assert(f.update_marker(stripe));
  assert(f.state(0) > 0.59f);
  near(f.covariance(1, 1), py, 1e-7f);
  stripe.gx = 6;
  assert(!f.update_marker(stripe));
  stripe.gx = 5;
  assert(f.update_marker(stripe) ==
         false); // Valid index, implausible jump is gated.
  f.reset(0.99f, 0.5f, 0);
  assert(f.update_marker(stripe)); // Boundary line index 5.
  FusionEngine engine;
  SensorEvent e{};
  e.kind = SensorKind::IMU;
  e.timestamp_us = 1000000;
  assert(engine.process(e, e.timestamp_us));
  e.kind = SensorKind::MAG;
  e.mag = {45, 0, 0};
  assert(
      !engine.process(e, e.timestamp_us)); // Unconfirmed magnetic cleanliness.
  e.timestamp_us += 10000;
  e.magnetic_clean = true;
  assert(engine.process(e, e.timestamp_us));
  e.kind = SensorKind::MARKER;
  e.timestamp_us += 10000;
  e.marker = {MarkerGeometry::X_LINE, 1, true, 2, -1, 1};
  assert(engine.process(e, e.timestamp_us));
  auto snap = engine.snapshot(e.timestamp_us);
  assert(snap.last_x_fix_us > 0 && snap.last_y_fix_us == 0 && !snap.tracking);
  e.timestamp_us += 10000;
  assert(!engine.process(
      e, e.timestamp_us)); // Same observation with a new timestamp.
  assert(engine.snapshot(e.timestamp_us).stats.duplicate == 1);
  std::puts("PASS four-state hold model, partition-invariant process noise, "
            "axis freshness, replay gate");
}
/** @brief Verify TCS color debounce, rearming, ambiguity and saturation
 * rejection.
 * @returns No value; aborts on classification/event regression.
 */
void test_color() {
  using namespace loc;
  const ColorReference refs[] = {
      {0.8f, 0.1f, 0.1f, MarkerGeometry::CELL, 1, 2}};
  ColorTracker tracker;
  MarkerSample out{};
  RgbcSample raw{800, 100, 100, 1200, 1, false};
  assert(!tracker.process(raw, refs, 1, &out));
  assert(!tracker.process(raw, refs, 1,
                          &out)); // Duplicate frame cannot satisfy debounce.
  raw.sequence = 2;
  assert(!tracker.process(raw, refs, 1, &out));
  raw.sequence = 3;
  assert(tracker.process(raw, refs, 1, &out));
  assert(out.observation_id == 1 && out.gx == 1 && out.gy == 2);
  raw.sequence = 4;
  assert(!tracker.process(raw, refs, 1, &out));
  raw.saturated = true;
  for (unsigned k = 5; k <= 7; ++k) {
    raw.sequence = k;
    assert(!tracker.process(raw, refs, 1, &out));
  }
  raw.saturated = false;
  for (unsigned k = 8; k < 10; ++k) {
    raw.sequence = k;
    assert(!tracker.process(raw, refs, 1, &out));
  }
  raw.sequence = 10;
  assert(tracker.process(raw, refs, 1, &out));
  assert(out.observation_id == 2);
  const ColorReference ambiguous[] = {refs[0], refs[0]};
  ColorTracker other;
  for (unsigned k = 1; k <= 8; ++k) {
    raw.sequence = k;
    assert(!other.process(raw, ambiguous, 2, &out));
  }
  std::puts("PASS TCS34725 calibrated classification, fresh-frame debounce, "
            "entry IDs, ambiguity");
}
/** @brief Simulate an accelerating trajectory with biased noisy IMU and ToF.
 * @returns No value; aborts on excessive error or unstable covariance.
 */
void test_trajectory() {
  using namespace loc;
  RobotEKF f;
  f.reset(0.28f, 0.38f, 0.04f);
  MazeMap arena = MazeMap::q1();
  for (int row = 0; row < 5; ++row)
    for (int col = 0; col < 5; ++col)
      arena.walls[row][col] = (row == 0 ? NORTH : 0) | (row == 4 ? SOUTH : 0) |
                              (col == 0 ? WEST : 0) | (col == 4 ? EAST : 0);
  assert(f.set_maze(arena));
  float x = 0.25f, y = 0.35f, yaw = 0, vx = 0;
  constexpr float dt = 0.01f;
  for (int i = 0; i < 800; ++i) {
    const float ax = i < 100 ? 0.1f : (i >= 400 && i < 500 ? -0.1f : 0);
    const float gz = i < 200 ? 0.12f : (i < 400 ? -0.12f : 0);
    const float c = std::cos(yaw), s = std::sin(yaw);
    ImuSample imu{};
    imu.ax_mps2 = c * ax + 0.003f * std::sin(i * 0.13f);
    imu.ay_mps2 = -s * ax + 0.003f * std::cos(i * 0.11f);
    imu.gz_rps = gz + 0.008f;
    assert(f.predict(imu, dt));
    x += vx * dt + 0.5f * ax * dt * dt;
    vx += ax * dt;
    yaw = wrap_pi(yaw + gz * dt);
    if (i % 5 == 0) {
      const float heading = yaw + 0.003f * std::sin(i * 0.1f);
      assert(
          f.update_mag({45 * std::cos(heading), -45 * std::sin(heading), 0}));
      for (int sensor = 0; sensor < 3; ++sensor) {
        const float range = RobotEKF::expected_range(arena, x, y, yaw,
                                                     cfg::lidar_mounts[sensor]);
        f.update_lidar(range + 0.002f * std::sin(i * 0.07f + sensor),
                       cfg::lidar_mounts[sensor]);
      }
    }
    check_covariance(f);
  }
  near(f.state(RobotEKF::X), x, 0.025f);
  near(f.state(RobotEKF::Y), y, 0.025f);
  near(wrap_pi(f.state(RobotEKF::YAW) - yaw), 0, deg2rad(3));
  near(f.state(RobotEKF::BGZ), 0.008f, 0.01f);
  std::printf("PASS trajectory: xy error=(%.4f,%.4f)m, yaw error=%.2fdeg\n",
              f.state(RobotEKF::X) - x, f.state(RobotEKF::Y) - y,
              wrap_pi(f.state(RobotEKF::YAW) - yaw) * 180 / kPi);
}
/** @brief Exercise real dt, held IMU input, stale data and tracking validity.
 * @returns No value; aborts on timestamp or validity regression.
 */
void test_time() {
  using namespace loc;
  FusionEngine engine;
  SensorEvent imu{};
  imu.kind = SensorKind::IMU;
  imu.timestamp_us = 1000000;
  imu.imu.gz_rps = 1;
  assert(engine.process(imu, imu.timestamp_us));
  SensorEvent mag{};
  mag.kind = SensorKind::MAG;
  mag.timestamp_us = 1010000;
  mag.magnetic_clean = true;
  mag.mag = {45 * std::cos(0.01f), -45 * std::sin(0.01f), 0};
  assert(engine.process(mag, mag.timestamp_us));
  near(engine.snapshot(mag.timestamp_us).yaw_rad, 0.01f, 1e-5f);
  imu.timestamp_us = 1020000;
  imu.imu.gz_rps = 0;
  assert(engine.process(imu, imu.timestamp_us));
  near(engine.snapshot(imu.timestamp_us).yaw_rad, 0.02f, 1e-5f);
  assert(!engine.process(imu, imu.timestamp_us));
  SensorEvent late{};
  late.kind = SensorKind::TOF;
  late.tof_id = TofId::FRONT;
  late.timestamp_us = 1015000;
  late.range_m = 0.43f;
  assert(!engine.process(late, 1020000));
  late.timestamp_us = 500000;
  assert(!engine.process(late, 1020000));
  late.timestamp_us = 2000000;
  assert(!engine.process(late, 1020000));
  imu.timestamp_us = 2000000;
  assert(engine.process(imu, imu.timestamp_us));
  const PoseSnapshot p = engine.snapshot(2000000);
  assert(p.stats.duplicate == 1 && p.stats.out_of_order == 1 &&
         p.stats.stale == 1);
  assert(p.stats.invalid == 1 && p.stats.timing_gaps == 1);
  assert(!p.tracking && !p.grid_confident);
  FusionEngine tracking;
  for (int i = 0; i < 300; ++i) {
    imu.timestamp_us = 3000000 + i * 10000;
    imu.imu = {};
    assert(tracking.process(imu, imu.timestamp_us));
    if (i % 5 == 0) {
      mag.timestamp_us = imu.timestamp_us;
      mag.mag = {45, 0, 0};
      assert(tracking.process(mag, mag.timestamp_us));
      for (int sensor = 0; sensor < 3; ++sensor) {
        SensorEvent tof{};
        tof.kind = SensorKind::TOF;
        tof.timestamp_us = imu.timestamp_us;
        tof.tof_id = static_cast<TofId>(sensor);
        tof.range_m =
            RobotEKF::expected_range(0.5f, 0.5f, 0, cfg::lidar_mounts[sensor]);
        assert(tracking.process(tof, tof.timestamp_us));
      }
    }
  }
  assert(tracking.snapshot(imu.timestamp_us).tracking);
  assert(tracking.snapshot(imu.timestamp_us).grid_confident);
  assert(!tracking.snapshot(imu.timestamp_us + 1000000).tracking);
  std::puts(
      "PASS event timing, duplicate/stale/future rejection, gaps, freshness");
}
/** @brief Verify open-loop startup, clipping, reversal and zero-on-fault
 * contracts.
 * @returns No value; aborts on a PWM/command/timing regression.
 */
void test_motor() {
  using namespace motor;
  WheelController c;
  int64_t now = 1000000;
  auto p = c.update({6, -4, now},
                    now); // First valid command drives without feedback.
  assert(p.status == ControlStatus::RUNNING && p.duty_left > 0 &&
         p.duty_right < 0);
  for (int i = 0; i < 100; ++i) {
    now += 10000;
    p = c.update({6, -4, now}, now);
  }
  near(p.duty_left, .30f, 1e-6f);
  near(p.duty_right, -.20f, 1e-6f);
  for (int i = 0; i < 200; ++i) {
    now += 10000;
    p = c.update({30, -30, now}, now);
  }
  near(p.duty_left, cfg::max_duty, 1e-6f);
  near(p.duty_right, -cfg::max_duty, 1e-6f);
  bool crossed_left = false, crossed_right = false;
  for (int i = 0; i < 200; ++i) {
    now += 10000;
    p = c.update({-6, 4, now}, now);
    if (p.duty_left == 0)
      crossed_left = true;
    if (p.duty_right == 0)
      crossed_right = true;
    if (p.duty_left < 0)
      assert(crossed_left);
    if (p.duty_right > 0)
      assert(crossed_right);
  }
  assert(crossed_left && crossed_right);
  now += 10000;
  p = c.update({0, 0, now}, now);
  assert(p.status == ControlStatus::IDLE && p.duty_left == 0 &&
         p.duty_right == 0);
  now += 10000;
  p = c.update({6, 6, now - cfg::command_timeout_us - 1}, now);
  assert(p.status == ControlStatus::COMMAND_TIMEOUT && p.duty_left == 0 &&
         p.duty_right == 0);
  for (const WheelCommand bad :
       {WheelCommand{31, 0, now + 10000},
        WheelCommand{0, std::numeric_limits<float>::quiet_NaN(),
                     now + 10000}}) {
    now += 10000;
    auto cmd = bad;
    cmd.timestamp_us = now;
    p = c.update(cmd, now);
    assert(p.status == ControlStatus::INVALID_COMMAND && p.duty_left == 0 &&
           p.duty_right == 0);
  }
  now += 10000;
  p = c.update({6, 6, now + 1}, now);
  assert(p.status == ControlStatus::INVALID_COMMAND && p.duty_left == 0);
  now += 100000;
  p = c.update({6, 6, now}, now);
  assert(p.status == ControlStatus::TIMING_FAULT && p.duty_left == 0);
  now += 10000;
  p = c.update({6, 6, now}, now);
  assert(p.duty_left > 0);
  p = c.update({6, 6, now}, now);
  assert(p.status == ControlStatus::TIMING_FAULT && p.duty_left == 0);
  now += 10000;
  p = c.update({}, now);
  assert(p.status == ControlStatus::IDLE && p.duty_left == 0);
  std::puts("PASS open-loop PWM: no encoder, cap, reversal through zero, "
            "coast, timeout, invalid command, task timing");
}
} // namespace

/** @brief Run all portable core regression tests.
 * @returns Zero if all assertions pass; aborts otherwise.
 */
int main() {
  test_measurements();
  test_event_model();
  test_color();
  test_trajectory();
  test_time();
  test_motor();
  std::puts("All portable core tests passed.");
  return 0;
}
