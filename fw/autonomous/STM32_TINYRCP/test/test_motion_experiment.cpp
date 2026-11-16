// SPDX-License-Identifier: GPL-3.0-only
#include "motor_control/wheel_controller.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/right_hand_rule.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
loc::MazeMap arena() {
  auto map = loc::MazeMap::q1();
  for (int row = 0; row < 5; ++row)
    for (int col = 0; col < 5; ++col)
      map.walls[row][col] =
          (row == 0 ? loc::NORTH : 0) | (row == 4 ? loc::SOUTH : 0) |
          (col == 0 ? loc::WEST : 0) | (col == 4 ? loc::EAST : 0);
  return map;
}
nav::BootstrapFrame frame(int64_t now) {
  nav::BootstrapFrame f{};
  f.imu_us = now;
  for (unsigned i = 0; i < 3; ++i) {
    f.tof_us[i] = now;
    f.range_m[i] = .3f;
  }
  return f;
}
loc::PoseSnapshot pose(int64_t now) {
  loc::PoseSnapshot p{};
  p.x_m = p.y_m = .5f;
  p.grid_x = p.grid_y = 2;
  p.tracking = p.grid_confident = true;
  p.timestamp_us = now;
  return p;
}
motor::MotorSnapshot moving(int64_t now) {
  motor::MotorSnapshot m{};
  m.timestamp_us = now;
  m.status = motor::ControlStatus::RUNNING;
  m.duty_left = m.duty_right = .5f;
  return m;
}
void scaling_and_motor() {
  for (const auto &input :
       {nav::DriveRequest{.21f, .25f}, nav::DriveRequest{.02f, -1.8f},
        nav::DriveRequest{-.15f, .4f}}) {
    const auto out = nav::motion_test::full_scale(input);
    assert(std::fabs(out.linear_m_s * input.angular_rad_s -
                     input.linear_m_s * out.angular_rad_s) < 1e-6f);
    const float l =
        (out.linear_m_s - out.angular_rad_s * motor::cfg::track_width_m / 2) /
        motor::cfg::wheel_radius_m;
    const float r =
        (out.linear_m_s + out.angular_rad_s * motor::cfg::track_width_m / 2) /
        motor::cfg::wheel_radius_m;
    assert(std::fabs(l) <= motor::cfg::max_wheel_rad_s);
    assert(std::fabs(r) <= motor::cfg::max_wheel_rad_s);
    assert(std::max(std::fabs(l), std::fabs(r)) >
           motor::cfg::max_wheel_rad_s - 1e-4f);
    motor::WheelController controller;
    auto state = controller.update({l, r, 1000000}, 1000000);
    const float pwm =
        std::max(std::fabs(state.duty_left), std::fabs(state.duty_right));
    if constexpr (nav::motion_test::enabled)
      assert(std::lround(pwm * 255) == 255);
    else
      assert(pwm < .2f);
    state = controller.update({0, 0, 1010000}, 1010000);
    assert(state.duty_left == 0 && state.duty_right == 0);
  }
  const auto stopped = nav::motion_test::full_scale({});
  assert(stopped.linear_m_s == 0 && stopped.angular_rad_s == 0);
}
void feature_turn_and_veering() {
  const auto map = arena();
  int64_t now = 1000000;
  auto p = pose(now);
  p.yaw_rad = loc::kPi / 2;
  nav::PathExecutor executor;
  assert(executor.begin(map, {4, 2}, p, now));
  auto out = executor.update(p, moving(now), frame(now), now);
  assert(out.angular_rad_s != 0);
  assert((out.linear_m_s > 0) == nav::motion_test::rolling_turns);
  now += 20000;
  p.timestamp_us = now;
  p.yaw_rad = 0;
  out = executor.update(p, moving(now), frame(now), now);
  assert((out.linear_m_s > 0) == nav::motion_test::rolling_turns);
  if constexpr (nav::motion_test::rolling_turns) {
    now += 20000;
    p.timestamp_us = now;
    p.yaw_rad = loc::deg2rad(30);
    out = executor.update(p, moving(now), frame(now), now);
    assert(out.linear_m_s > 0 && out.angular_rad_s != 0);
    assert(out.diagnostics.decision ==
           (nav::motion_test::nonstop_only
                ? nav::ExecuteDecision::TURN_TO_HEADING
                : nav::ExecuteDecision::HEADING_RECOVERY));
  }

  float steering[2];
  for (unsigned i = 0; i < 2; ++i) {
    p = pose(now);
    p.y_m += i == 0 ? .02f : -.02f;
    nav::PathExecutor veering;
    assert(veering.begin(map, {4, 2}, p, now));
    out = veering.update(p, moving(now), frame(now), now);
    assert(out.linear_m_s > 0);
    steering[i] = nav::motion_test::full_scale(out).angular_rad_s;
  }
  assert(steering[0] < 0 && steering[1] > 0);

  p = pose(now);
  nav::PathExecutor corner;
  assert(corner.begin(map, {3, 3}, p, now));
  const auto target = corner.path()[1];
  p.yaw_rad = std::atan2(float(target.y - 2), float(target.x - 2));
  assert(corner.begin(map, {3, 3}, p, now));
  p.x_m = (target.x + .5f) * loc::kCellM;
  p.y_m = (target.y + .5f) * loc::kCellM;
  p.grid_x = target.x;
  p.grid_y = target.y;
  out = corner.update(p, moving(now), frame(now), now);
  assert((out.linear_m_s > 0) == nav::motion_test::rolling_turns);
  if constexpr (nav::motion_test::rolling_turns) {
    now += 20000;
    p.timestamp_us = now;
    out = corner.update(p, moving(now), frame(now), now);
    assert(out.linear_m_s > 0 && out.angular_rad_s != 0);
  }
}
void rhr_turn_and_veering() {
  nav::RightHandRule controller;
  int64_t now = 1000000;
  auto f = frame(now);
  auto p = pose(now);
  p.x_m += .003f; // Past the qualified center, inside its acceptance window.
  f.range_m[2] = .2f;
  controller.begin();
  auto out = controller.update(f, moving(now), now, &p);
  assert((out.linear_m_s > 0) == nav::motion_test::rolling_turns);
  if constexpr (nav::motion_test::rolling_turns) {
    assert(out.angular_rad_s < 0);
    for (unsigned i = 0; i < 8; ++i) {
      now += 20000;
      f = frame(now);
      f.imu.gz_rps = -10;
      out = controller.update(f, moving(now), now, &p);
      assert(out.linear_m_s > 0);
    }
    assert(controller.phase() == nav::FallbackPhase::CLEAR);
    nav::RightHandRule centering;
    centering.begin();
    f = frame(now);
    f.range_m[0] = .06f;
    f.range_m[2] = .02f;
    out = centering.update(f, moving(now), now);
    assert(out.linear_m_s > 0 && out.angular_rad_s > 0);
  }
}
void independent_turn_heading() {
  if constexpr (!nav::motion_test::nonstop_only)
    return;
  const int64_t now = 1000000;
  auto p = pose(now);
  p.yaw_rad = loc::deg2rad(30);
  nav::PathExecutor executor;
  assert(executor.begin(arena(), {4, 2}, p, now));
  nav::HeadingObservation heading{};
  heading.valid = true;
  heading.timestamp_us = now;
  heading.yaw_rad = 0;
  auto out = executor.update(p, moving(now), frame(now), now, &heading);
  assert(out.linear_m_s > 0 && out.angular_rad_s == 0);
  // Heading may finish a turn, but cannot substitute for translation tracking.
  p.tracking = false;
  nav::PathExecutor untracked;
  auto start = p;
  start.tracking = true;
  assert(untracked.begin(arena(), {4, 2}, start, now));
  out = untracked.update(p, moving(now), frame(now), now, &heading);
  assert(out.status == nav::ExecuteStatus::PAUSED);
  assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
}
void retained_stops() {
  int64_t now = 1000000;
  auto p = pose(now);
  const auto map = arena();
  nav::PathExecutor final;
  assert(final.begin(map, {3, 2}, p, now));
  p.x_m = .7f;
  p.grid_x = 3;
  const auto stop = final.update(p, moving(now), frame(now), now);
  assert(stop.status == nav::ExecuteStatus::SETTLE);
  assert(stop.linear_m_s == 0 && stop.angular_rad_s == 0);

  if constexpr (nav::motion_test::nonstop_only) {
    // The lower-speed mode must still wait for measured pivot clearance.
    nav::PathExecutor blocked;
    p = pose(now);
    p.x_m = .04f;
    p.grid_x = 0;
    p.yaw_rad = loc::kPi / 2;
    assert(blocked.begin(map, {4, 2}, p, now));
    auto out = blocked.update(p, moving(now), frame(now), now);
    assert(out.status == nav::ExecuteStatus::PAUSED);
    assert(out.diagnostics.decision ==
           nav::ExecuteDecision::WAIT_TURN_CLEARANCE);
    assert(out.linear_m_s == 0 && out.angular_rad_s == 0);

    nav::RightHandRule rhr;
    rhr.begin_uturn();
    out = rhr.update(frame(now), moving(now), now);
    assert(out.status == nav::ExecuteStatus::PAUSED); // No center evidence.
    assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
    rhr.begin();
    auto f = frame(now);
    f.range_m[0] = .01f;
    out = rhr.update(f, moving(now), now);
    assert(out.status == nav::ExecuteStatus::PAUSED); // Side stop remains.
    assert(out.linear_m_s == 0 && out.angular_rad_s == 0);

    nav::ExecutorConfig invalid;
    invalid.max_speed_m_s = .3f;
    nav::PathExecutor capped(invalid);
    assert(!capped.begin(map, {4, 2}, pose(now), now));
  }
}

} // namespace
int main() {
  scaling_and_motor();
  feature_turn_and_veering();
  rhr_turn_and_veering();
  retained_stops();
  independent_turn_heading();
  std::printf("PASS motion profile max=%u nonstop=%u: PWM, curvature, moving "
              "turns, veering and zero stop\n",
              unsigned(nav::motion_test::enabled),
              unsigned(nav::motion_test::nonstop_only));
}
