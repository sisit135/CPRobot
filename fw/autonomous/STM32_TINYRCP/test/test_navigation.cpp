// SPDX-License-Identifier: GPL-3.0-only

#include "AStar.hpp"
#include "localization/config.hpp"
#include "navigation/map_resolver.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/path_executor.hpp"
#include "navigation/turn_clearance.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <queue>
namespace {
void near(float a, float b) {
  assert(std::isfinite(a) && std::fabs(a - b) < 1e-5f);
}
loc::MazeMap arena() {
  auto m = loc::MazeMap::q1();
  for (int r = 0; r < 5; ++r)
    for (int c = 0; c < 5; ++c)
      m.walls[r][c] = (r == 0 ? loc::NORTH : 0) | (r == 4 ? loc::SOUTH : 0) |
                      (c == 0 ? loc::WEST : 0) | (c == 4 ? loc::EAST : 0);
  return m;
}
void geometry() {
  // Independent wall-intersection checks make the side sensors' forward
  // offset observable; cardinal-only rays cannot distinguish that offset.
  const auto open = arena();
  for (float yaw : {-loc::deg2rad(10), loc::deg2rad(10)}) {
    for (unsigned sensor : {0u, 2u}) {
      const auto &mount = loc::cfg::lidar_mounts[sensor];
      near(mount.x_body_m, .0225f);
      const float side = sensor == 0 ? 1.f : -1.f;
      const float expected = .5f / std::cos(yaw) -
                             loc::cfg::platform_half_side_m -
                             side * .0225f * std::tan(yaw);
      const auto hit = loc::trace_maze_ray(open, .5f, .5f, yaw, mount);
      near(hit.range, expected);
      near(nav::cardinal_wall_clearance(hit.range, mount, yaw), .5f);
    }
  }
  auto m = loc::MazeMap::q1();
  assert(m.contradictions() == 5);
  assert(!(m.walls[3][1] & loc::SOUTH));
  assert(!(m.walls[4][1] & loc::NORTH));
  assert(m.edge({1, 1}, loc::SOUTH) == loc::EdgeState::OPEN);
  assert(m.edge({1, 0}, loc::NORTH) == loc::EdgeState::OPEN);
  assert(m.can_step({1, 1}, {1, 0}) && m.can_step({1, 0}, {1, 1}));
  auto passage = loc::trace_maze_ray(m, .3f, .3f, -loc::kPi / 2, {0, 0, 0});
  assert(passage.status == loc::RayStatus::HIT);
  near(passage.range, .3f); // Continue through y=.2 to the south arena wall.
  auto h = loc::trace_maze_ray(m, .5f, .5f, 0, loc::cfg::lidar_mounts[1]);
  near(h.range, .3f - loc::cfg::lidar_mounts[1].x_body_m);
  assert(h.axis == 0 && !h.ambiguous);
  near(loc::trace_maze_ray(m, .5f, .5f, 0, loc::cfg::lidar_mounts[0]).range,
       .1f - loc::cfg::platform_half_side_m);
  near(loc::trace_maze_ray(m, .5f, .5f, 0, loc::cfg::lidar_mounts[2]).range,
       .3f - loc::cfg::platform_half_side_m);
  assert(m.edge({1, 1}, loc::EAST) ==
         loc::EdgeState::CLOSED); // One-sided wall.
  auto a = arena();
  h = loc::trace_maze_ray(a, .5f, .5f, 0, {0, 0, 0});
  near(h.range, .5f);
  assert(!h.ambiguous);
  h = loc::trace_maze_ray(a, .5f, .5f, loc::kPi / 4, {0, 0, 0});
  assert(h.ambiguous);
  assert(loc::trace_maze_ray(a, 1, .5f, 0, {0, 0, 0}).status ==
         loc::RayStatus::INVALID);
  for (auto &r : a.known)
    for (auto &v : r)
      v = 0;
  assert(loc::trace_maze_ray(a, .5f, .5f, 0, {0, 0, 0}).status ==
         loc::RayStatus::UNKNOWN_EDGE);
  a = arena();
  for (auto &r : a.walls)
    for (auto &v : r)
      v = 0;
  assert(loc::trace_maze_ray(a, .5f, .5f, 0, {0, 0, 0}).status ==
         loc::RayStatus::NO_HIT);
  for (unsigned k = 0; k < 4; ++k) {
    auto rot = m.rotated(k, static_cast<loc::MapKind>(k));
    float x = .5f, y = .5f;
    for (unsigned j = 0; j < k; ++j) {
      float nx = 1 - y;
      y = x;
      x = nx;
    }
    h = loc::trace_maze_ray(rot, x, y, k * loc::kPi / 2,
                            loc::cfg::lidar_mounts[1]);
    near(h.range, .3f - loc::cfg::lidar_mounts[1].x_body_m);
    assert(rot.contradictions() == 5);
    const auto upper = loc::rotate_cell({1, 1}, k);
    const auto lower = loc::rotate_cell({1, 0}, k);
    assert(rot.can_step(upper, lower) && rot.can_step(lower, upper));
  }
  // Differentiate a stable wall model independently of the EKF covariance code.
  const float d = 1e-4f, x = .49f, y = .49f, yaw = .1f;
  const auto mount = loc::cfg::lidar_mounts[1];
  h = loc::trace_maze_ray(m, x, y, yaw, mount);
  assert(!h.ambiguous && h.axis == 0);
  float num = (loc::trace_maze_ray(m, x + d, y, yaw, mount).range -
               loc::trace_maze_ray(m, x - d, y, yaw, mount).range) /
              (2 * d);
  assert(std::fabs(num + 1 / std::cos(yaw)) < .001f);
}
void planning() {
  const auto m = loc::MazeMap::q1();
  AStar::Generator gen;
  assert(gen.setMaze(m));
  assert(!gen.setDiagonalMovement(true));
  for (int source = 0; source < 25; ++source) {
    int distances[25];
    for (auto &d : distances)
      d = -1;
    distances[source] = 0;
    std::queue<int> q;
    q.push(source);
    while (!q.empty()) {
      int a = q.front();
      q.pop();
      loc::Cell c{a % 5, a / 5};
      for (auto d : {loc::Cell{1, 0}, loc::Cell{-1, 0}, loc::Cell{0, 1},
                     loc::Cell{0, -1}}) {
        loc::Cell b{c.x + d.x, c.y + d.y};
        if (!m.can_step(c, b))
          continue;
        int id = b.y * 5 + b.x;
        if (distances[id] < 0) {
          distances[id] = distances[a] + 1;
          q.push(id);
        }
      }
    }
    for (int target = 0; target < 25; ++target) {
      auto p = gen.findPath({source % 5, source / 5}, {target % 5, target / 5});
      if (distances[target] < 0) {
        assert(p.empty());
        assert(gen.status() == AStar::PathStatus::NO_PATH);
      } else {
        assert(int(p.size()) == distances[target] + 1);
        assert((p[0] == loc::Cell{source % 5, source / 5}));
        assert((p[p.size() - 1] == loc::Cell{target % 5, target / 5}));
        for (unsigned i = 1; i < p.size(); ++i)
          assert(m.can_step(p[i - 1], p[i]));
      }
    }
  }
  assert(gen.findPath({5, 0}, {0, 0}).empty());
  auto unknown = m;
  for (auto &r : unknown.known)
    for (auto &v : r)
      v = 0;
  gen.setMaze(unknown);
  assert(gen.findPath({0, 0}, {0, 1}).empty());
  gen.setMaze(m);
  gen.addCollision({0, 1});
  assert(gen.findPath({0, 0}, {0, 2}).empty());
}
nav::BootstrapFrame frame(const loc::MazeMap &m, loc::Cell c, float yaw,
                          int64_t now, unsigned seq) {
  nav::BootstrapFrame f{};
  f.color = {800, 100, 100, 1200, seq, false};
  f.color_us = f.imu_us = f.mag_us = now;
  f.mag = {45 * std::cos(yaw), -45 * std::sin(yaw), 0};
  f.magnetic_clean = true;
  for (int i = 0; i < 3; ++i) {
    f.tof_us[i] = now;
    f.range_m[i] = loc::trace_maze_ray(m, (c.x + .5f) * .2f, (c.y + .5f) * .2f,
                                       yaw, loc::cfg::lidar_mounts[i])
                       .range;
  }
  return f;
}
void resolution() {
  auto config = nav::default_resolver_config();
  nav::MapResolver uncalibrated(config);
  assert(uncalibrated.update({}, 1000000).status ==
         nav::ResolveStatus::WAIT_CALIBRATION);
  config.starts[0] = {{0, 0}, nav::TileClass::BLUE_START, 15};
  config.start_count = 1;
  config.colors[0] = {.8f, .1f, .1f, nav::TileClass::BLUE_START};
  config.color_count = 1;
  config.colors_calibrated = true;
  for (unsigned k = 0; k < 4; ++k) {
    nav::MapResolver resolver(config);
    auto c = loc::rotate_cell({0, 0}, k);
    nav::Resolution result;
    for (unsigned i = 1; i <= 3; ++i) {
      auto f = frame(config.variants[k], c, loc::kPi / 2 + k * loc::kPi / 2,
                     1000000 + i * 50000, i);
      result = resolver.update(f, f.color_us);
      auto duplicate = resolver.update(f, f.color_us);
      assert(duplicate.confirmations == result.confirmations);
    }
    if (result.status != nav::ResolveStatus::RESOLVED)
      std::printf("resolver k=%u status=%u score=%g margin=%g\n", k,
                  unsigned(result.status), result.score, result.margin);
    assert(result.status == nav::ResolveStatus::RESOLVED &&
           unsigned(result.kind) == k && result.cell == c);
  }
  // Identical maps/POIs must remain ambiguous instead of guessing a quadrant.
  for (unsigned k = 0; k < 4; ++k) {
    config.variants[k] = config.variants[0];
    config.variants[k].kind = static_cast<loc::MapKind>(k);
    config.turns[k] = 0;
  }
  nav::MapResolver ambiguous(config);
  auto f = frame(config.variants[0], {0, 0}, loc::kPi / 2, 1000000, 1);
  assert(ambiguous.update(f, 1000000).status == nav::ResolveStatus::AMBIGUOUS);
  assert(ambiguous.update(f, 2000000).status ==
         nav::ResolveStatus::WAIT_SENSORS);
}
void settling() {
  auto map = arena();
  loc::PoseSnapshot p{};
  p.x_m = .1f;
  p.y_m = .1f;
  p.yaw_rad = 0;
  p.grid_x = p.grid_y = 0;
  p.tracking = true;
  p.timestamp_us = 1000000;
  motor::MotorSnapshot motor{};
  nav::BootstrapFrame f{};
  nav::PathExecutor executor;
  assert(executor.begin(map, {0, 1}, p, 1000000));
  p.yaw_rad = loc::kPi / 2; // A real pivot has just reached its target heading.
  if constexpr (nav::motion_test::rolling_turns) {
    motor.timestamp_us = f.imu_us = f.tof_us[1] = p.timestamp_us;
    f.range_m[1] = .5f;
    const auto drive = executor.update(p, motor, f, p.timestamp_us);
    assert(drive.status == nav::ExecuteStatus::ADVANCE);
    assert(drive.linear_m_s > 0 && drive.angular_rad_s == 0);
    return;
  }
  // Zero PWM is a request, not proof of standstill: quiet fresh IMU and dwell
  // matter.
  for (int k = 0; k <= 22; ++k) {
    const int64_t now = 1000000 + k * 20000;
    p.timestamp_us = motor.timestamp_us = f.imu_us = now;
    auto drive = executor.update(p, motor, f, now);
    if (k < 22)
      assert(drive.status == nav::ExecuteStatus::TURN);
    else
      assert(drive.status == nav::ExecuteStatus::ADVANCE);
    assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
  }
  executor.cancel();
  p.timestamp_us = 2000000;
  p.yaw_rad = 0;
  assert(executor.begin(map, {0, 1}, p, 2000000));
  p.yaw_rad = loc::kPi / 2;
  f.imu_us = 2000000;
  for (int k = 0; k <= 50; ++k) {
    const int64_t now = 2000000 + k * 20000;
    p.timestamp_us = motor.timestamp_us = now;
    auto drive = executor.update(p, motor, f, now);
    assert(drive.status == nav::ExecuteStatus::TURN && drive.linear_m_s == 0);
  }
  // Fresh but rotating gyro also prevents phase transition despite zero PWM.
  for (int k = 1; k <= 30; ++k) {
    const int64_t now = 3000000 + k * 20000;
    p.timestamp_us = motor.timestamp_us = f.imu_us = now;
    f.imu.gz_rps = .2f;
    auto drive = executor.update(p, motor, f, now);
    assert(drive.status == nav::ExecuteStatus::TURN);
  }
  // Moving/jittering poses revoke confirmations without resetting elapsed
  // zero-PWM/quiet-gyro coast time. Three fresh stable poses are still needed.
  f.imu.gz_rps = 0;
  nav::DriveRequest drive{};
  int64_t now = 3600000;
  for (int k = 0; k <= 30; ++k) {
    now += 20000;
    p.x_m = k % 2 ? .11f : .09f;
    p.timestamp_us = motor.timestamp_us = f.imu_us = now;
    drive = executor.update(p, motor, f, now);
    assert(drive.status == nav::ExecuteStatus::TURN);
  }
  assert(drive.diagnostics.zero_dwell_us >= 400000 &&
         drive.diagnostics.confirmations == 0);
  for (int k = 0; k < 4; ++k) {
    now += 20000;
    p.x_m = .1f;
    p.timestamp_us = motor.timestamp_us = f.imu_us = now;
    drive = executor.update(p, motor, f, now);
    assert(drive.status ==
           (k < 3 ? nav::ExecuteStatus::TURN : nav::ExecuteStatus::ADVANCE));
  }
  std::puts("PASS no-encoder settling: zero PWM dwell, fresh quiet gyro, "
            "distinct snapshots");
}
void corner_turn_clearance() {
  const auto map = arena();
  loc::PoseSnapshot p{};
  p.x_m = p.y_m = .1f;
  p.grid_x = p.grid_y = 0;
  p.tracking = true;
  p.timestamp_us = 1000000;
  nav::ExecutorConfig config;
  config.position_tolerance_m = .010f;
  nav::PathExecutor executor(config);
  assert(executor.begin(map, {1, 1}, p, p.timestamp_us));
  const auto target = executor.path()[1];
  const float dx = float(target.x), dy = float(target.y);
  p.yaw_rad = std::atan2(dy, dx);
  motor::MotorSnapshot motor{};
  nav::BootstrapFrame f{};
  f.range_m[1] = .5f;
  int64_t now = p.timestamp_us;
  auto step = [&](nav::PathExecutor &control) {
    now += 20000;
    p.timestamp_us = motor.timestamp_us = f.imu_us = f.tof_us[1] = now;
    return control.update(p, motor, f, now);
  };
  for (int k = 0; k < 30; ++k)
    step(executor);
  assert(executor.status() == nav::ExecuteStatus::ADVANCE);
  p.grid_x = target.x;
  p.grid_y = target.y;
  auto position = [&](float passage) {
    p.x_m = (target.x + .5f) * loc::kCellM + dx * passage;
    p.y_m = (target.y + .5f) * loc::kCellM + dy * passage;
  };
  // An open branch permits a safe turn 8 mm before the center. Requiring
  // exact/past-center alignment here used to add several seconds of creep.
  position(-.008f);
  auto drive = step(executor);
  if constexpr (nav::motion_test::rolling_turns) {
    assert(drive.status == nav::ExecuteStatus::TURN && drive.linear_m_s > 0);
    assert(drive.angular_rad_s != 0 && drive.diagnostics.pivot_clear);
    return;
  }
  assert(drive.status == nav::ExecuteStatus::SETTLE && drive.linear_m_s == 0);
  assert(drive.diagnostics.pivot_clear && !drive.diagnostics.passed_center);
  for (int k = 0; k < 20; ++k) {
    position(k % 2 ? -.006f : -.008f);
    drive = step(executor);
    assert(drive.status == nav::ExecuteStatus::SETTLE &&
           drive.linear_m_s == 0 && drive.angular_rad_s == 0);
  }
  for (int k = 0; k < 10 && drive.status != nav::ExecuteStatus::TURN; ++k)
    drive = step(executor);
  assert(drive.status == nav::ExecuteStatus::TURN && drive.waypoint == 2);
}
void settled_corner_clearance_wait() {
  const auto map = arena();
  loc::PoseSnapshot p{};
  p.x_m = p.y_m = .1f;
  p.grid_x = p.grid_y = 0;
  p.tracking = true;
  p.timestamp_us = 1000000;
  nav::ExecutorConfig config;
  config.position_tolerance_m = .010f;
  nav::PathExecutor executor(config);
  assert(executor.begin(map, {1, 1}, p, p.timestamp_us));
  const auto target = executor.path()[1];
  const float dx = float(target.x), dy = float(target.y);
  p.yaw_rad = std::atan2(dy, dx);
  motor::MotorSnapshot motor{};
  nav::BootstrapFrame frame{};
  frame.range_m[1] = .5f;
  int64_t now = p.timestamp_us;
  auto step = [&]() {
    now += 20000;
    p.timestamp_us = motor.timestamp_us = frame.imu_us = frame.tof_us[1] = now;
    return executor.update(p, motor, frame, now);
  };
  for (int k = 0; k < 30; ++k)
    step();
  p.grid_x = target.x;
  p.grid_y = target.y;
  const float center_x = (target.x + .5f) * loc::kCellM;
  const float center_y = (target.y + .5f) * loc::kCellM;
  p.x_m = center_x;
  p.y_m = center_y;
  if constexpr (nav::motion_test::rolling_turns) {
    assert(step().status == nav::ExecuteStatus::TURN);
    p.x_m = center_x - dy * .012f;
    p.y_m = center_y - dx * .012f;
    const auto drive = step();
    assert(drive.status == nav::ExecuteStatus::PAUSED);
    assert(drive.diagnostics.decision ==
           nav::ExecuteDecision::WAIT_TURN_CLEARANCE);
    assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
    return;
  }
  assert(step().status == nav::ExecuteStatus::SETTLE);
  // GDB seed 3371016718: a stable arrival 6 mm off-center must reach
  // the next guarded TURN instead of waiting forever on its swept circle.
  frame.range_m[1] = .020f;
  p.x_m = center_x - dy * .006f;
  p.y_m = center_y - dx * .006f;
  auto drifted = executor, expired = executor;
  auto drifting_pose = p;
  drifting_pose.x_m -= dx * .020f;
  drifting_pose.y_m -= dy * .020f;
  auto drive = drifted.update(drifting_pose, motor, frame, now);
  assert(drive.status == nav::ExecuteStatus::ADVANCE);
  assert(drive.diagnostics.decision == nav::ExecuteDecision::REAPPROACH_CENTER);
  drive = expired.update(p, motor, frame, now + config.leg_timeout_us + 1);
  assert(drive.status == nav::ExecuteStatus::FAULT &&
         drive.fault == nav::ExecuteFault::LEG_TIMEOUT);
  // A stale pose and a rotating gyro cannot complete the stopped arrival.
  for (int k = 0; k < 25; ++k) {
    now += 20000;
    motor.timestamp_us = frame.imu_us = now;
    frame.imu.gz_rps = .2f;
    drive = executor.update(p, motor, frame, now);
    assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
    assert(executor.status() == nav::ExecuteStatus::SETTLE);
  }
  frame.imu.gz_rps = 0;
  for (int k = 0; k < 40 && executor.status() != nav::ExecuteStatus::TURN;
       ++k) {
    const float along = k % 2 ? .001f : -.001f;
    p.x_m = center_x - dy * .006f + dx * along;
    p.y_m = center_y - dx * .006f + dy * along;
    drive = step();
    assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
  }
  assert(executor.status() == nav::ExecuteStatus::TURN && drive.waypoint == 2);
  // Confirming arrival never licenses an unproved pivot. The independent
  // executor clearance wait is now visible to the task-owned TurnGuard.
  p.x_m = center_x - dy * .012f;
  p.y_m = center_y - dx * .012f;
  drive = step();
  assert(drive.status == nav::ExecuteStatus::PAUSED);
  assert(drive.diagnostics.decision ==
         nav::ExecuteDecision::WAIT_TURN_CLEARANCE);
  assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
}
void starting_uturn_clearance() {
  const auto map = arena();
  loc::PoseSnapshot p{};
  p.x_m = p.y_m = .1f;
  p.grid_x = p.grid_y = 0;
  p.yaw_rad = -loc::kPi / 2; // Facing the starting south wall, route is north.
  p.tracking = true;
  p.timestamp_us = 1000000;
  nav::PathExecutor executor;
  assert(executor.begin(map, {0, 1}, p, p.timestamp_us));
  motor::MotorSnapshot motor{};
  motor.timestamp_us = p.timestamp_us;
  nav::BootstrapFrame f{};
  f.imu_us = p.timestamp_us;
  // A close front wall must not suppress an otherwise safe stationary U-turn.
  f.range_m[1] = .1f - loc::cfg::lidar_mounts[1].x_body_m;
  f.tof_us[1] = p.timestamp_us;
  auto drive = executor.update(p, motor, f, p.timestamp_us);
  assert(drive.status == nav::ExecuteStatus::TURN);
  assert((drive.linear_m_s > 0) == nav::motion_test::rolling_turns);
  assert(std::fabs(drive.angular_rad_s) > 0);
  assert(drive.diagnostics.pivot_clear);
  // Move close enough that the selected body's swept diagonal strikes the
  // west wall while its unrotated lateral footprint still clears that wall.
  p.x_m = robot::geometry::footprint_radius_m - .005f;
  drive = executor.update(p, motor, f, p.timestamp_us);
  assert(drive.status == nav::ExecuteStatus::PAUSED);
  assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
  assert(drive.diagnostics.decision ==
         nav::ExecuteDecision::WAIT_TURN_CLEARANCE);
  // A 2 mm offset leaves room for both the diagonal and a positive wall margin.
  p.x_m = .098f;
  drive = executor.update(p, motor, f, p.timestamp_us);
  assert(drive.status == nav::ExecuteStatus::TURN && drive.angular_rad_s != 0);
}
void long_front_ray_keeps_two_observable_axes() {
  const auto map = arena();
  loc::PoseSnapshot p{};
  p.x_m = .876f;
  p.y_m = .224f;
  p.grid_x = 4;
  p.grid_y = 1;
  p.yaw_rad = loc::kPi / 2;
  p.tracking = p.grid_confident = true;
  p.timestamp_us = 1000000;
  nav::PathExecutor executor;
  assert(executor.begin(map, {4, 4}, p, p.timestamp_us));
  motor::MotorSnapshot motor{};
  motor.timestamp_us = p.timestamp_us;
  nav::BootstrapFrame f{};
  f.imu_us = p.timestamp_us;
  for (int i = 0; i < 3; ++i) {
    f.tof_us[i] = p.timestamp_us;
    f.range_m[i] = loc::trace_maze_ray(map, p.x_m, p.y_m, p.yaw_rad,
                                       loc::cfg::lidar_mounts[i])
                       .range;
  }
  const auto out = executor.update(p, motor, f, p.timestamp_us);
  assert(out.status == nav::ExecuteStatus::ADVANCE && out.linear_m_s > 0);
  unsigned axes = 0;
  const float course = p.yaw_rad + out.angular_rad_s / 2;
  for (const auto &mount : loc::cfg::lidar_mounts) {
    const auto hit = loc::trace_maze_ray(map, p.x_m, p.y_m, course, mount);
    if (!hit.ambiguous)
      axes |= 1U << hit.axis;
  }
  assert(axes == 3);
  // Short-wall approaches retain the stronger cross-track correction.
  f.range_m[1] = .2f;
  const auto short_wall = executor.update(p, motor, f, p.timestamp_us);
  assert(std::fabs(short_wall.angular_rad_s) > std::fabs(out.angular_rad_s));
}
void measured_cross_track_steering() {
  const auto map = arena();
  loc::PoseSnapshot p{};
  p.x_m = .098f;
  p.y_m = .1f;
  p.grid_x = p.grid_y = 0;
  p.yaw_rad = loc::kPi / 2;
  p.tracking = true;
  p.timestamp_us = 1000000;
  nav::PathExecutor executor;
  assert(executor.begin(map, {0, 1}, p, p.timestamp_us));
  motor::MotorSnapshot motor{};
  nav::BootstrapFrame f{};
  f.range_m[1] = .5f;
  int64_t now = p.timestamp_us;
  auto step = [&]() {
    now += 20000;
    p.timestamp_us = motor.timestamp_us = f.imu_us = f.tof_us[1] = now;
    return executor.update(p, motor, f, now);
  };
  for (int k = 0; k < 30; ++k)
    step();
  auto drive = step();
  assert(drive.status == nav::ExecuteStatus::ADVANCE && drive.linear_m_s > 0);
  assert(drive.angular_rad_s < 0); // East toward x=.1 while advancing north.
  p.x_m = .102f;
  drive = step();
  assert(drive.angular_rad_s > 0); // West toward the same measured target.
  assert(std::fabs(drive.angular_rad_s) <= 2 * loc::deg2rad(10));
}
void fresh_side_range_preserves_observability() {
  auto map = arena();
  loc::PoseSnapshot p{};
  p.x_m = p.y_m = .5f;
  p.grid_x = p.grid_y = 2;
  p.tracking = true;
  p.timestamp_us = 1000000;
  nav::ExecutorConfig config;
  config.max_speed_m_s = .18f;
  nav::PathExecutor control(config);
  assert(control.begin(map, {3, 2}, p, p.timestamp_us));
  motor::MotorSnapshot m{};
  nav::BootstrapFrame f{};
  f.range_m[0] = .04f;
  f.range_m[1] = .5f;
  f.range_m[2] = .022f; // Still valid, approaching the 15 mm minimum.
  auto step = [&]() {
    p.timestamp_us += 20000;
    m.timestamp_us = f.imu_us = p.timestamp_us;
    for (auto &t : f.tof_us)
      t = p.timestamp_us;
    return control.update(p, m, f, p.timestamp_us);
  };
  auto out = step();
  assert(out.linear_m_s > 0 && out.linear_m_s <= .060f);
  assert(out.angular_rad_s > 0); // Move away from the right wall early.
  std::swap(f.range_m[0], f.range_m[2]);
  out = step();
  assert(out.linear_m_s <= .060f && out.angular_rad_s < 0);
  // Range-based correction never overrides invalid localization or the
  // front stop. A fresh timestamp alone does not make an invalid pose usable.
  p.tracking = false;
  out = step();
  assert(out.status == nav::ExecuteStatus::PAUSED && out.linear_m_s == 0 &&
         out.angular_rad_s == 0);
  p.tracking = true;
  f.range_m[1] = .020f;
  out = step();
  assert(out.status == nav::ExecuteStatus::BLOCKED && out.linear_m_s == 0);
}
void close_wall_replan_uses_bounded_course_correction() {
  for (unsigned rotation = 0; rotation < 4; ++rotation) {
    auto map = loc::MazeMap::q1().rotated(rotation,
                                          static_cast<loc::MapKind>(rotation));
    loc::PoseSnapshot p{};
    p.x_m = .122f;
    p.y_m = .812f;
    for (unsigned i = 0; i < rotation; ++i) {
      const float x = p.x_m;
      p.x_m = 1 - p.y_m;
      p.y_m = x;
    }
    const auto cell = loc::rotate_cell({0, 4}, rotation);
    p.grid_x = cell.x;
    p.grid_y = cell.y;
    p.yaw_rad = loc::wrap_pi(-1.69f + rotation * loc::kPi / 2);
    p.tracking = p.grid_confident = true;
    p.timestamp_us = 1000000;
    motor::MotorSnapshot m{};
    m.timestamp_us = p.timestamp_us;
    nav::BootstrapFrame f{};
    f.imu_us = p.timestamp_us;
    f.range_m[0] = .017f;
    f.range_m[1] = .14f;
    f.range_m[2] = .05f;
    for (auto &stamp : f.tof_us)
      stamp = p.timestamp_us;
    nav::PathExecutor control;
    const auto goal = loc::rotate_cell({0, 3}, rotation);
    assert(control.begin(map, goal, p, p.timestamp_us));
    const auto out = control.update(p, m, f, p.timestamp_us);
    assert(out.status == nav::ExecuteStatus::ADVANCE && out.linear_m_s > 0);
    assert(out.angular_rad_s < 0); // Move toward the center and away from left.
    p.yaw_rad += loc::deg2rad(30);
    assert(control.begin(map, goal, p, p.timestamp_us));
    assert(control.status() == nav::ExecuteStatus::TURN); // Large errors pivot.
  }
}

void measured_progress_watchdog() {
  auto map = arena();
  loc::PoseSnapshot p{};
  p.x_m = p.y_m = .5f;
  p.grid_x = p.grid_y = 2;
  p.tracking = true;
  int64_t now = p.timestamp_us = 1000000;
  nav::ExecutorConfig config;
  config.leg_timeout_us = 1000000;
  nav::PathExecutor control(config);
  assert(control.begin(map, {3, 2}, p, now));
  motor::MotorSnapshot m{};
  nav::BootstrapFrame f{};
  f.range_m[1] = .5f;
  auto step = [&]() {
    now += 100000;
    p.timestamp_us = m.timestamp_us = f.imu_us = f.tof_us[1] = now;
    return control.update(p, m, f, now);
  };
  // Measured progress continues beyond the nominal leg interval.
  for (int i = 0; i < 16; ++i) {
    p.x_m += .003f;
    const auto out = step();
    assert(out.status == nav::ExecuteStatus::ADVANCE && out.linear_m_s > 0);
  }
  auto stopped = control, stale = control, duplicate = control;
  const float x = p.x_m;
  const int64_t anchor = now;
  nav::DriveRequest out{};
  for (int i = 0; i < 12; ++i) {
    now += 100000;
    p.timestamp_us = m.timestamp_us = f.imu_us = f.tof_us[1] = now;
    p.x_m = x + (i % 2 ? .001f : -.001f);
    out = stopped.update(p, m, f, now);
  }
  assert(out.status == nav::ExecuteStatus::FAULT &&
         out.fault == nav::ExecuteFault::LEG_TIMEOUT);
  p.x_m = x + .05f;
  p.timestamp_us = anchor; // A stale/duplicate apparent jump is no progress.
  out = stale.update(p, m, f, now);
  assert(out.status == nav::ExecuteStatus::FAULT &&
         out.fault == nav::ExecuteFault::LEG_TIMEOUT);
  out = duplicate.update(p, m, f, now);
  assert(out.status == nav::ExecuteStatus::FAULT);
}

void straight_waypoint_does_not_stop() {
  loc::PoseSnapshot p{};
  p.x_m = p.y_m = .1f;
  p.grid_x = p.grid_y = 0;
  p.yaw_rad = loc::kPi / 2;
  p.tracking = true;
  p.timestamp_us = 1000000;
  nav::PathExecutor executor;
  assert(executor.begin(arena(), {0, 2}, p, p.timestamp_us));
  motor::MotorSnapshot motor{};
  nav::BootstrapFrame f{};
  f.range_m[1] = .5f;
  int64_t now = p.timestamp_us;
  for (int k = 0; k < 30; ++k) {
    now += 20000;
    p.timestamp_us = motor.timestamp_us = f.imu_us = f.tof_us[1] = now;
    executor.update(p, motor, f, now);
  }
  p.grid_y = 1;
  p.y_m = .291f;
  auto stale = executor, blocked = executor, crossed = executor,
       too_far = executor;
  auto drive = executor.update(p, motor, f, now);
  assert(drive.status == nav::ExecuteStatus::ADVANCE && drive.waypoint == 2);
  assert(drive.linear_m_s > 0 &&
         drive.diagnostics.decision == nav::ExecuteDecision::PASS_WAYPOINT);
  // Each measured component is within arrival tolerance, while the radial
  // error is 28 mm. A straight continuation can hand off across this boundary.
  auto passed = p;
  passed.x_m = .122f;
  passed.y_m = .318f;
  drive = crossed.update(passed, motor, f, now);
  assert(drive.status == nav::ExecuteStatus::ADVANCE && drive.waypoint == 2);
  assert(drive.diagnostics.decision == nav::ExecuteDecision::PASS_WAYPOINT);
  passed.y_m = .340f;
  drive = too_far.update(passed, motor, f, now);
  assert(drive.status == nav::ExecuteStatus::FAULT &&
         drive.fault == nav::ExecuteFault::TARGET_PASSED);
  f.tof_us[1] -= 200000;
  drive = stale.update(p, motor, f, now);
  assert(drive.status == nav::ExecuteStatus::PAUSED && drive.linear_m_s == 0);
  f.tof_us[1] = now;
  f.range_m[1] = .01f;
  drive = blocked.update(p, motor, f, now);
  assert(drive.status == nav::ExecuteStatus::BLOCKED && drive.linear_m_s == 0);
}
void corridor_is_one_linear_leg() {
  auto map = arena();
  for (int y = 1; y <= 2; ++y) {
    map.walls[4 - y][0] |= loc::EAST;
    map.walls[4 - y][1] |= loc::WEST;
  }
  loc::PoseSnapshot p{};
  p.x_m = p.y_m = .1f;
  p.grid_x = p.grid_y = 0;
  p.yaw_rad = loc::kPi / 2;
  p.tracking = true;
  p.timestamp_us = 1000000;
  nav::PathExecutor executor;
  assert(executor.begin(map, {0, 4}, p, p.timestamp_us));
  motor::MotorSnapshot motor{};
  nav::BootstrapFrame f{};
  f.range_m[1] = .5f;
  int64_t now = p.timestamp_us;
  nav::DriveRequest drive{};
  p.timestamp_us = motor.timestamp_us = f.imu_us = f.tof_us[1] = now;
  drive = executor.update(p, motor, f, now);
  // Even an aligned initial route/replan needs no zero-PWM posture dwell.
  assert(drive.status == nav::ExecuteStatus::ADVANCE && drive.linear_m_s > 0);
  assert(drive.waypoint == 3 && !drive.diagnostics.stop_required);
  for (int k = 0; k < 30; ++k) {
    now += 20000;
    p.timestamp_us = motor.timestamp_us = f.imu_us = f.tof_us[1] = now;
    drive = executor.update(p, motor, f, now);
  }
  assert(drive.waypoint == 3); // Skip the two featureless corridor centers.
  p.grid_y = 1;
  p.y_m = .3f;
  drive = executor.update(p, motor, f, now);
  assert(drive.status == nav::ExecuteStatus::ADVANCE && drive.waypoint == 3);
  assert(drive.linear_m_s > 0 && drive.diagnostics.distance_m > .39f);
  // The side opening at cell 3 is the next mapped observation landmark; it
  // does not force a stop when the route continues in the same direction.
  p.grid_y = 3;
  p.y_m = .691f;
  drive = executor.update(p, motor, f, now);
  assert(drive.waypoint == 4 && drive.linear_m_s > 0);
  assert(drive.diagnostics.decision == nav::ExecuteDecision::PASS_WAYPOINT);
  now += 20000;
  p.timestamp_us = motor.timestamp_us = f.imu_us = f.tof_us[1] = now;
  drive = executor.update(p, motor, f, now);
  assert(drive.diagnostics.stop_required); // Final endpoint retains braking.
}
void measured_approach_gain() {
  loc::PoseSnapshot p{};
  p.x_m = .1f;
  p.y_m = .27f;
  p.grid_x = 0;
  p.grid_y = 1;
  p.yaw_rad = loc::kPi / 2;
  p.tracking = true;
  p.timestamp_us = 1000000;
  nav::ExecutorConfig config;
  config.max_speed_m_s = .18f;
  config.approach_gain_per_s = 4.f;
  config.position_tolerance_m = .010f;
  nav::PathExecutor executor(config);
  assert(executor.begin(arena(), {0, 2}, p, p.timestamp_us));
  motor::MotorSnapshot motor{};
  nav::BootstrapFrame f{};
  f.range_m[1] = .5f;
  auto step = [&]() {
    p.timestamp_us += 20000;
    motor.timestamp_us = f.imu_us = f.tof_us[1] = p.timestamp_us;
    return executor.update(p, motor, f, p.timestamp_us);
  };
  assert(step().linear_m_s == config.max_speed_m_s);
  p.grid_y = 2;
  p.y_m = .47f;
  near(step().linear_m_s, .12f); // 30 mm measured remaining x 4 / s.
  p.y_m = .491f;
  const auto arrived = step();
  assert(arrived.status == nav::ExecuteStatus::SETTLE &&
         arrived.linear_m_s == 0);
  for (float invalid : {0.f, -1.f, 10.1f, INFINITY, NAN}) {
    config.approach_gain_per_s = invalid;
    nav::PathExecutor bad(config);
    assert(!bad.begin(arena(), {0, 3}, p, p.timestamp_us));
    assert(bad.fault() == nav::ExecuteFault::INVALID_START);
  }
}
} // namespace
int main() {
  long_front_ray_keeps_two_observable_axes();
  geometry();
  planning();
  resolution();
  settling();
  corner_turn_clearance();
  settled_corner_clearance_wait();
  starting_uturn_clearance();
  measured_cross_track_steering();
  fresh_side_range_preserves_observability();
  measured_progress_watchdog();
  close_wall_replan_uses_bounded_course_correction();
  straight_waypoint_does_not_stop();
  corridor_is_one_linear_leg();
  measured_approach_gain();
  std::puts("PASS wall geometry, all-pairs A*/BFS, four variants, ambiguity "
            "and freshness");
}
