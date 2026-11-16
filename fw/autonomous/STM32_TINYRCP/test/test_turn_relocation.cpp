// SPDX-License-Identifier: GPL-3.0-only
#include "motor_control/wheel_controller.hpp"
#include "navigation/turn_relocation.hpp"
#include "robot_sim.hpp"
#include <cassert>
#include <cstdio>
#include <limits>

namespace {
loc::MazeMap arena() {
  auto m = loc::MazeMap::q1();
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x)
      m.walls[4 - y][x] = (y == 0 ? loc::SOUTH : 0) |
                          (y == 4 ? loc::NORTH : 0) | (x == 0 ? loc::WEST : 0) |
                          (x == 4 ? loc::EAST : 0);
  return m;
}
loc::PoseSnapshot pose(loc::Cell cell, float yaw) {
  loc::PoseSnapshot p{};
  p.x_m = (cell.x + .5f) * loc::kCellM;
  p.y_m = (cell.y + .5f) * loc::kCellM;
  p.grid_x = cell.x;
  p.grid_y = cell.y;
  p.yaw_rad = yaw;
  p.tracking = p.grid_confident = true;
  p.timestamp_us = 1000000;
  return p;
}
uint32_t bit(loc::Cell c) { return uint32_t(1) << (c.y * 5 + c.x); }
void rank_and_rotate() {
  for (unsigned r = 0; r < 4; ++r) {
    const auto map = arena().rotated(r, static_cast<loc::MapKind>(r));
    auto p = pose(loc::rotate_cell({2, 2}, r), r * loc::kPi / 2);
    auto plan = nav::plan_turn_relocation(map, p, loc::rotate_cell({4, 2}, r));
    assert(plan.valid && plan.cell == loc::rotate_cell({3, 2}, r));
    assert(plan.travel_steps == 1 && plan.extra_steps == 0 && !plan.reverse);
    // Prefer the farther cell on an optimal continuation over a closer
    // opening that would require going away from the goal and back.
    plan = nav::plan_turn_relocation(map, p, loc::rotate_cell({4, 2}, r),
                                     bit(loc::rotate_cell({3, 2}, r)));
    assert(plan.valid && plan.cell == loc::rotate_cell({4, 2}, r));
    assert(plan.travel_steps == 2 && plan.extra_steps == 0);
    plan = nav::plan_turn_relocation(map, p, loc::rotate_cell({0, 2}, r));
    assert(plan.valid && plan.cell == loc::rotate_cell({1, 2}, r));
    assert(plan.reverse && plan.travel_steps == 1 && plan.extra_steps == 0);
  }
}
void closest_uses_measured_distance_when_detours_tie() {
  const auto map = arena();
  auto p = pose({2, 2}, 0);
  p.x_m = .41f;
  const auto plan = nav::plan_turn_relocation(map, p, {2, 4});
  assert(plan.valid && plan.reverse && plan.cell == (loc::Cell{1, 2}));
  assert(plan.extra_steps == 2 && std::fabs(plan.travel_m - .11f) < .0001f);
}

void corridor_opening_and_unknown_edges() {
  auto map = arena();
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x)
      map.walls[4 - y][x] |= loc::EAST | loc::WEST;
  map.walls[1][2] &= ~loc::EAST;
  map.walls[1][3] &= ~loc::WEST;
  auto p = pose({2, 1}, loc::kPi / 2);
  auto plan = nav::plan_turn_relocation(map, p, {3, 3});
  assert(plan.valid && plan.cell == (loc::Cell{2, 3}) &&
         plan.travel_steps == 2);
  assert(plan.extra_steps == 0 && !plan.reverse);
  map.known[1][2] &= ~loc::EAST;
  map.known[1][3] &= ~loc::WEST;
  assert(!nav::plan_turn_relocation(map, p, {3, 3}).valid);
  // Unknown rear travel is forbidden even when the goal is otherwise reachable.
  map = arena();
  p = pose({2, 2}, 0);
  map.known[2][2] &= ~loc::WEST;
  map.known[2][1] &= ~loc::EAST;
  assert(!nav::plan_turn_relocation(map, p, {0, 2}, bit({3, 2}) | bit({4, 2}))
              .valid);
}
void rejects_unproved_inputs_and_geometry() {
  auto map = arena();
  auto p = pose({0, 0}, 0);
  p.x_m = .05f; // Topology is open but the actual chassis overlaps a boundary.
  assert(!nav::plan_turn_relocation(map, p, {4, 0}).valid);
  p = pose({2, 2}, loc::deg2rad(10));
  assert(!nav::plan_turn_relocation(map, p, {4, 2}).valid);
  p = pose({2, 2}, 0);
  p.tracking = false;
  assert(!nav::plan_turn_relocation(map, p, {4, 2}).valid);
  p.tracking = true;
  p.yaw_rad = std::numeric_limits<float>::quiet_NaN();
  assert(!nav::plan_turn_relocation(map, p, {4, 2}).valid);
}
loc::MazeMap opening_map() {
  auto map = arena();
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x)
      map.walls[4 - y][x] |= loc::EAST | loc::WEST;
  map.walls[1][2] &= ~loc::EAST;
  map.walls[1][3] &= ~loc::WEST;
  return map;
}
void physical_relocation(unsigned rotation, bool reverse) {
  const auto map =
      opening_map().rotated(rotation, static_cast<loc::MapKind>(rotation));
  const auto start =
      loc::rotate_cell(reverse ? loc::Cell{2, 4} : loc::Cell{2, 1}, rotation);
  const auto goal = loc::rotate_cell({3, 3}, rotation);
  const float yaw = (rotation + 1) * loc::kPi / 2;
  sim::Noise noise{};
  noise.gyro_sigma = noise.gyro_bias = noise.mag_sigma = noise.range_sigma = 0;
  sim::Surface surface;
  surface.enabled = true;
  sim::Robot robot(map, start, yaw, 71, noise, {}, surface);
  nav::TurnRelocation recovery;
  motor::WheelController wheels;
  motor::MotorSnapshot m{};
  int64_t now = 1000000;
  auto measured = [&]() {
    auto p = pose({int(robot.x / loc::kCellM), int(robot.y / loc::kCellM)},
                  robot.yaw);
    p.x_m = robot.x;
    p.y_m = robot.y;
    p.timestamp_us = now;
    return p;
  };
  assert(recovery.begin(map, measured(), goal, now));
  assert(recovery.plan().reverse == reverse);
  bool moved = false;
  for (int i = 0; i < 2000 && !recovery.complete(); ++i) {
    now += 10000;
    m.timestamp_us = now;
    const auto out =
        recovery.update(map, measured(), robot.sensors(now, i + 1), m, now);
    assert(out.status != nav::ExecuteStatus::BLOCKED);
    if (out.linear_m_s != 0) {
      moved = true;
      assert((out.linear_m_s < 0) == reverse);
    }
    const float b = motor::cfg::track_width_m / 2;
    m = wheels.update(
        {(out.linear_m_s - b * out.angular_rad_s) / motor::cfg::wheel_radius_m,
         (out.linear_m_s + b * out.angular_rad_s) / motor::cfg::wheel_radius_m,
         now},
        now);
    robot.step(m.duty_left, m.duty_right, .01f);
    // Independent corner bounds against the fixture's straight corridor.
    const float h = loc::cfg::platform_half_side_m;
    for (float sx : {-1.f, 1.f})
      for (float sy : {-1.f, 1.f}) {
        float x =
            robot.x + h * (sx * std::cos(robot.yaw) - sy * std::sin(robot.yaw));
        float y =
            robot.y + h * (sx * std::sin(robot.yaw) + sy * std::cos(robot.yaw));
        for (unsigned r = 0; r < rotation; ++r) {
          const float previous = x;
          x = y;
          y = 1 - previous;
        }
        assert(x > .4f && x < .6f && y > 0 && y < 1);
      }
    assert(!robot.collided);
  }
  assert(recovery.complete() && moved);
  assert(std::hypot(robot.vx_m_s, robot.vy_m_s) < .001f &&
         std::fabs(robot.body_omega_rps) < .01f);
  std::printf("PASS physical relocation rotation=%u reverse=%d\n", rotation,
              reverse);
}
void execution_faults_stop() {
  static_assert(sizeof(nav::TurnRelocation) <= 128,
                "Bounded task recovery state");
  auto map = arena();
  auto p = pose({2, 2}, 0);
  int64_t now = p.timestamp_us;
  nav::TurnRelocation recovery;
  assert(recovery.begin(map, p, {4, 2}, now));
  motor::MotorSnapshot m{};
  m.timestamp_us = now;
  nav::BootstrapFrame f{};
  f.imu_us = now;
  for (int i = 0; i < 3; ++i) {
    f.tof_us[i] = now;
    f.range_m[i] = .2f;
  }
  auto out = recovery.update(map, p, f, m, now);
  assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
  for (int tick = 0; tick < 50; ++tick) {
    now += 10000;
    p.timestamp_us = m.timestamp_us = f.imu_us = now;
    for (auto &stamp : f.tof_us)
      stamp = now;
    out = recovery.update(map, p, f, m, now);
  }
  assert(out.linear_m_s >
         0); // Exercise faults after actual travel is authorized.
  p.tracking = false;
  out = recovery.update(map, p, f, m, now);
  assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
  p.tracking = true;
  f.tof_us[0] -= 150001;
  out = recovery.update(map, p, f, m, now);
  assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
  f.tof_us[0] = now;
  m.status = motor::ControlStatus::DRIVER_FAULT;
  out = recovery.update(map, p, f, m, now);
  assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
  out = recovery.update(map, p, f, m, now + 20000001);
  assert(out.status == nav::ExecuteStatus::BLOCKED && out.linear_m_s == 0 &&
         out.angular_rad_s == 0);
  recovery.cancel();
  assert(!recovery.busy() && !recovery.complete());
  out = recovery.update(map, p, f, m, now);
  assert(out.linear_m_s == 0 && out.angular_rad_s == 0);
  // A newly closed wall within the coast envelope must clear both commands,
  // including when a nonzero request has already been computed this frame.
  recovery.cancel();
  map = arena();
  p = pose({2, 2}, 0);
  now = p.timestamp_us;
  assert(recovery.begin(map, p, {4, 2}, now));
  m = {};
  f = {};
  for (int tick = 0; tick < 50; ++tick) {
    now += 10000;
    p.timestamp_us = m.timestamp_us = f.imu_us = now;
    for (int i = 0; i < 3; ++i) {
      f.tof_us[i] = now;
      f.range_m[i] = .2f;
    }
    out = recovery.update(map, p, f, m, now);
  }
  assert(out.linear_m_s > 0);
  p.x_m = .540f;
  map.walls[2][2] |= loc::EAST;
  map.walls[2][3] |= loc::WEST;
  out = recovery.update(map, p, f, m, now);
  assert(out.status == nav::ExecuteStatus::BLOCKED && out.linear_m_s == 0 &&
         out.angular_rad_s == 0);
  recovery.cancel();
  map = arena();
  p = pose({2, 2}, 0);
  for (int attempt = 0; attempt < 3; ++attempt) {
    assert(recovery.begin(map, p, {4, 2}, p.timestamp_us));
    recovery.finish();
  }
  assert(!recovery.begin(map, p, {4, 2}, p.timestamp_us));
}

void resumed_route_avoids_failed_turn_cell() {
  const auto map = arena();
  const auto p = pose({1, 2}, 0);
  nav::PathExecutor executor;
  assert(executor.begin(map, {4, 2}, p, p.timestamp_us, bit({2, 2})));
  assert(executor.path().count > 4);
  for (auto cell : executor.path())
    assert(cell != (loc::Cell{2, 2}));
  // Avoidance is temporary planning state, not a fabricated physical wall.
  assert(map.can_step({1, 2}, {2, 2}));
  assert(executor.begin(map, {2, 2}, p, p.timestamp_us, bit({2, 2})));
  assert(executor.path()[executor.path().count - 1] == (loc::Cell{2, 2}));
}

} // namespace
int main() {
  for (unsigned r = 0; r < 4; ++r) {
    physical_relocation(r, false);
    physical_relocation(r, true);
  }
  resumed_route_avoids_failed_turn_cell();
  execution_faults_stop();
  closest_uses_measured_distance_when_detours_tie();
  rank_and_rotate();
  corridor_opening_and_unknown_edges();
  rejects_unproved_inputs_and_geometry();
  std::puts("PASS passive opening selection: rotations, nearest optimal "
            "continuation, reverse, unknown edges, footprint rejection");
}
