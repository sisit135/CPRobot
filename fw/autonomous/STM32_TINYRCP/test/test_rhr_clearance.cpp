// SPDX-License-Identifier: GPL-3.0-only

#include "localization/config.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/right_hand_rule.hpp"
#include <cassert>
#include <cmath>
#include <initializer_list>

namespace {
loc::MazeMap open_arena_with_corner() {
  auto map = loc::MazeMap::q1();
  for (int y = 0; y < loc::kMazeN; ++y)
    for (int x = 0; x < loc::kMazeN; ++x) {
      map.walls[y][x] =
          (y == 0 ? loc::SOUTH : 0) | (y == loc::kMazeN - 1 ? loc::NORTH : 0) |
          (x == 0 ? loc::WEST : 0) | (x == loc::kMazeN - 1 ? loc::EAST : 0);
      map.known[y][x] = 15;
    }
  // A finite east-facing wall ends at the open south passage. This models
  // the inside corner at the side-opening edge.
  map.walls[2][2] |= loc::EAST;
  map.walls[2][3] |= loc::WEST;
  return map;
}

struct Fixture {
  static nav::RightHandConfig settings() {
    nav::RightHandConfig cfg;
    // Trigger the blocked-front branch before the 102 mm center fixture;
    // v2's front edge is 55 mm from origin, so use 50 mm physical clearance.
    cfg.front_stop_m = .050f;
    return cfg;
  }
  nav::RightHandRule rhr{settings()};
  nav::BootstrapFrame frame{};
  motor::MotorSnapshot motor{};
  nav::HeadingObservation heading{};
  int64_t now = 1000000;
  Fixture(float front, float left, float right) {
    rhr.begin();
    frame.range_m[0] = left;
    frame.range_m[1] = front;
    frame.range_m[2] = right;
    heading = {0, now, true};
    refresh();
  }
  nav::DriveRequest step(bool inhibited = false) {
    now += 20000;
    refresh();
    return rhr.update(frame, motor, now, nullptr, &heading, inhibited);
  }
  void refresh() {
    frame.imu_us = now;
    for (auto &stamp : frame.tof_us)
      stamp = now;
    motor.timestamp_us = now;
    heading.timestamp_us = now;
  }
  void decide_center() {
    for (int i = 0; i < 30 && rhr.phase() != nav::FallbackPhase::CENTER &&
                    rhr.phase() != nav::FallbackPhase::TURN;
         ++i)
      step();
    assert(rhr.phase() == nav::FallbackPhase::CENTER ||
           (nav::motion_test::rolling_turns &&
            rhr.phase() == nav::FallbackPhase::TURN));
  }
};

void centered_deadend_pivots_without_creeping() {
  // Front clearance is 100 mm from the chassis center; each side is clear
  // beyond the 97 mm swept-diagonal envelope.
  Fixture f(.100f - loc::cfg::lidar_mounts[1].x_body_m,
            .110f - loc::cfg::platform_half_side_m,
            .110f - loc::cfg::platform_half_side_m);
  f.decide_center();
  bool turned = false;
  for (int i = 0; i < 40; ++i) {
    const auto out = f.step();
    assert((out.linear_m_s > 0) == (nav::motion_test::rolling_turns &&
                                    f.rhr.phase() == nav::FallbackPhase::TURN));
    if (f.rhr.phase() == nav::FallbackPhase::TURN) {
      turned = true;
      break;
    }
  }
  assert(turned);
  const auto out = f.step();
  assert(out.angular_rad_s > 0); // Dead end selects the U-turn.
}

void early_pivot_when_all_measured_clearances_are_safe() {
  // The measured front wall is 102 mm away, just 2 mm before the nominal
  // center. The latched target is center+2 mm, so remaining is 4 mm.
  Fixture f(.102f - loc::cfg::lidar_mounts[1].x_body_m,
            .105f - loc::cfg::platform_half_side_m,
            .105f - loc::cfg::platform_half_side_m);
  f.decide_center();
  bool turned = false;
  for (int i = 0; i < 40; ++i) {
    const auto out = f.step();
    assert((out.linear_m_s > 0) == (nav::motion_test::rolling_turns &&
                                    f.rhr.phase() == nav::FallbackPhase::TURN));
    if (f.rhr.phase() == nav::FallbackPhase::TURN) {
      turned = true;
      break;
    }
  }
  assert(turned);
}

void insufficient_swept_clearance_never_turns_or_moves() {
  Fixture f(.102f - loc::cfg::lidar_mounts[1].x_body_m,
            .085f - loc::cfg::platform_half_side_m,
            .110f - loc::cfg::platform_half_side_m);
  f.decide_center();
  bool requested_guard = false;
  for (int i = 0; i < 40; ++i) {
    const auto out = f.step();
    if (out.diagnostics.decision == nav::ExecuteDecision::WAIT_TURN_CLEARANCE) {
      assert(out.diagnostics.pivot_angle_valid);
      assert(std::fabs(out.diagnostics.pivot_angle_rad - loc::kPi) < .001f);
    }
    requested_guard =
        requested_guard ||
        out.diagnostics.decision == nav::ExecuteDecision::WAIT_TURN_CLEARANCE;
    assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
    assert(f.rhr.phase() != nav::FallbackPhase::TURN);
  }
  assert(requested_guard);
}

void guard_hold_consumes_gyro_without_spending_pivot_budget() {
  Fixture f(.102f - loc::cfg::lidar_mounts[1].x_body_m,
            .105f - loc::cfg::platform_half_side_m, .14f);
  f.decide_center();
  for (int i = 0; i < 40 && f.rhr.phase() != nav::FallbackPhase::TURN; ++i)
    f.step();
  assert(f.rhr.phase() == nav::FallbackPhase::TURN);
  // A real guard correction can take longer than the eight-second active
  // turn bound. Gyro samples remain continuous during its bounded hold.
  for (int i = 0; i < 450; ++i) {
    const auto out = f.step(true);
    assert(out.status != nav::ExecuteStatus::FAULT);
    assert(f.rhr.phase() == nav::FallbackPhase::TURN);
  }
  // A mapped guard recovery lean may approach the side-range stop. RHR
  // consumes gyro while inhibited; it cannot issue an applied pivot itself.
  f.frame.range_m[0] = .014f;
  assert(f.step(true).status != nav::ExecuteStatus::FAULT);
  f.frame.range_m[0] = .105f - loc::cfg::platform_half_side_m;
  f.frame.imu.gz_rps = -.8f;
  // The completed right turn sees open front space. A fixed blocked-front
  // sample would immediately start another decision when coast is disabled.
  f.frame.range_m[1] = .5f;
  for (int i = 0; i < 110 && f.rhr.phase() == nav::FallbackPhase::TURN; ++i)
    f.step();
  assert(f.rhr.phase() == nav::FallbackPhase::CLEAR);

  // Guard ownership must not hide a real missing gyro interval.
  Fixture gap(.102f - loc::cfg::lidar_mounts[1].x_body_m,
              .105f - loc::cfg::platform_half_side_m, .14f);
  gap.decide_center();
  for (int i = 0; i < 40 && gap.rhr.phase() != nav::FallbackPhase::TURN; ++i)
    gap.step();
  assert(gap.rhr.phase() == nav::FallbackPhase::TURN);
  gap.now += 60000;
  assert(gap.step(true).status == nav::ExecuteStatus::FAULT);

  Fixture timeout(.102f - loc::cfg::lidar_mounts[1].x_body_m,
                  .105f - loc::cfg::platform_half_side_m, .14f);
  timeout.decide_center();
  for (int i = 0; i < 40 && timeout.rhr.phase() != nav::FallbackPhase::TURN;
       ++i)
    timeout.step();
  for (int i = 0; i < 410; ++i)
    timeout.step();
  assert(timeout.rhr.phase() == nav::FallbackPhase::FAULT);
}

void noisy_side_stop_preserves_only_continuous_quiet() {
  Fixture f(.5f, .04f, .014f);
  for (int i = 0; i < 30; ++i) {
    f.frame.range_m[2] = i % 2 ? .016f : .014f;
    const auto out = f.step();
    if (f.frame.range_m[2] <= .015f)
      assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
  }
  f.frame.range_m[2] = .016f;
  f.step();
  assert(f.rhr.phase() == nav::FallbackPhase::FOLLOW);
  assert(f.step().linear_m_s > 0);

  for (bool gap : {false, true}) {
    Fixture interrupted(.5f, .04f, .014f);
    for (int i = 0; i < 30; ++i)
      interrupted.step();
    if (gap)
      interrupted.now += 60000;
    else
      interrupted.motor.duty_left = .1f;
    interrupted.step();
    interrupted.motor.duty_left = 0;
    interrupted.frame.range_m[2] = .016f;
    const auto out = interrupted.step();
    if constexpr (nav::motion_test::rolling_turns) {
      assert(out.linear_m_s > 0);
      assert(interrupted.rhr.phase() == nav::FallbackPhase::FOLLOW);
    } else {
      assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
      assert(interrupted.rhr.phase() == nav::FallbackPhase::COAST);
    }
  }
}

void narrow_corridor_centers_both_sides_and_holds_heading() {
  Fixture f(.5f, .026f, .040f);
  for (int i = 0; i < 40 && f.rhr.phase() != nav::FallbackPhase::FOLLOW; ++i)
    f.step();
  assert(f.rhr.phase() == nav::FallbackPhase::FOLLOW);
  const auto centered = f.step();
  assert(centered.linear_m_s > 0 && centered.angular_rad_s < 0);
  // A fresh independent heading corrects a larger post-turn yaw error.
  f.heading.yaw_rad = .1f;
  assert(f.step().angular_rad_s < centered.angular_rad_s);
}

void side_opening_remains_available_for_early_pivot() {
  Fixture f(.102f - loc::cfg::lidar_mounts[1].x_body_m,
            .105f - loc::cfg::platform_half_side_m, .140f);
  f.decide_center();
  bool turned = false;
  for (int i = 0; i < 40; ++i) {
    const auto out = f.step();
    assert((out.linear_m_s > 0) == (nav::motion_test::rolling_turns &&
                                    f.rhr.phase() == nav::FallbackPhase::TURN));
    if (f.rhr.phase() == nav::FallbackPhase::TURN) {
      turned = true;
      break;
    }
  }
  assert(turned);
  const auto out = f.step();
  assert(out.angular_rad_s < 0); // Right-side opening selects a right turn.
}

void finite_side_opening_corner_requires_center_approach() {
  const auto map = open_arena_with_corner();
  constexpr float x = .450f, y = .500f, yaw = 0;
  Fixture f(.05f, .05f, .20f);
  f.frame.range_m[0] =
      loc::trace_maze_ray(map, x, y, yaw, loc::cfg::lidar_mounts[0]).range;
  f.frame.range_m[1] =
      loc::trace_maze_ray(map, x, y, yaw, loc::cfg::lidar_mounts[1]).range;
  f.frame.range_m[2] =
      loc::trace_maze_ray(map, x, y, yaw, loc::cfg::lidar_mounts[2]).range;
  assert(f.frame.range_m[2] >= .12f); // Side opening detected.
  assert(f.frame.range_m[1] > .045f); // Before front wall-stop threshold.
  f.decide_center();
  const auto out = f.step();
  assert(f.rhr.phase() == nav::FallbackPhase::CENTER);
  assert(out.linear_m_s > 0 && out.angular_rad_s == 0);
}
} // namespace

int main() {
  noisy_side_stop_preserves_only_continuous_quiet();
  centered_deadend_pivots_without_creeping();
  early_pivot_when_all_measured_clearances_are_safe();
  insufficient_swept_clearance_never_turns_or_moves();
  side_opening_remains_available_for_early_pivot();
  finite_side_opening_corner_requires_center_approach();
  guard_hold_consumes_gyro_without_spending_pivot_budget();
  narrow_corridor_centers_both_sides_and_holds_heading();
}
