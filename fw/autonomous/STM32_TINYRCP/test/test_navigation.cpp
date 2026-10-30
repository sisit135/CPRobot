#include "AStar.hpp"
#include "localization/config.hpp"
#include "navigation/map_resolver.hpp"
#include "navigation/path_executor.hpp"
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
  auto m = loc::MazeMap::q1();
  assert(m.contradictions() == 6);
  auto h = loc::trace_maze_ray(m, .5f, .5f, 0, loc::cfg::lidar_mounts[1]);
  near(h.range, .23f);
  assert(h.axis == 0 && !h.ambiguous);
  near(loc::trace_maze_ray(m, .5f, .5f, 0, loc::cfg::lidar_mounts[0]).range,
       .04f);
  near(loc::trace_maze_ray(m, .5f, .5f, 0, loc::cfg::lidar_mounts[2]).range,
       .24f);
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
    near(h.range, .23f);
    assert(rot.contradictions() == 6);
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
  p.yaw_rad = loc::kPi / 2;
  p.grid_x = p.grid_y = 0;
  p.tracking = true;
  p.timestamp_us = 1000000;
  motor::MotorSnapshot motor{};
  nav::BootstrapFrame f{};
  nav::PathExecutor executor;
  assert(executor.begin(map, {0, 1}, p, 1000000));
  // Zero PWM is a request, not proof of standstill: quiet fresh IMU and dwell
  // matter.
  for (int k = 0; k <= 23; ++k) {
    const int64_t now = 1000000 + k * 20000;
    p.timestamp_us = motor.timestamp_us = f.imu_us = now;
    auto drive = executor.update(p, motor, f, now);
    if (k < 23)
      assert(drive.status == nav::ExecuteStatus::TURN);
    else
      assert(drive.status == nav::ExecuteStatus::ADVANCE);
    assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
  }
  executor.cancel();
  p.timestamp_us = 2000000;
  assert(executor.begin(map, {0, 1}, p, 2000000));
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
  std::puts("PASS no-encoder settling: zero PWM dwell, fresh quiet gyro, "
            "distinct snapshots");
}
} // namespace
int main() {
  geometry();
  planning();
  resolution();
  settling();
  std::puts("PASS wall geometry, all-pairs A*/BFS, four variants, ambiguity "
            "and freshness");
}
