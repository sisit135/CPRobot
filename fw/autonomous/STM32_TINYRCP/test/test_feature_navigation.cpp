#include "localization/config.hpp"
#include "navigation/feature_navigation.hpp"
#include "navigation/map_resolver.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
namespace {
nav::BootstrapFrame observation(const loc::MazeMap &map, float x, float y,
                                float yaw, int64_t t) {
  nav::BootstrapFrame f{};
  f.imu_us = f.mag_us = t;
  f.magnetic_clean = true;
  f.mag = {45 * std::cos(yaw), -45 * std::sin(yaw), 0};
  for (int i = 0; i < 3; ++i) {
    f.tof_us[i] = t;
    f.range_m[i] =
        loc::trace_maze_ray(map, x, y, yaw, loc::cfg::lidar_mounts[i]).range;
  }
  return f; // Deliberately no color or startup POI.
}
loc::MazeMap arena() {
  auto m = loc::MazeMap::q1();
  for (int r = 0; r < 5; ++r)
    for (int c = 0; c < 5; ++c)
      m.walls[r][c] = (r == 0 ? loc::NORTH : 0) | (r == 4 ? loc::SOUTH : 0) |
                      (c == 0 ? loc::WEST : 0) | (c == 4 ? loc::EAST : 0);
  return m;
}
void tracking() {
  auto config = nav::default_resolver_config();
  config.variants[0] = arena();
  nav::FeatureConfig settings;
  settings.variant_mask = 1;
  nav::FeatureNavigation engine(config, settings);
  int64_t t = 1000000;
  nav::FeatureSnapshot s{};
  for (int i = 0; i < 3; ++i, t += 50000)
    s = engine.update(observation(config.variants[0], .1f, .1f, 0, t), t);
  assert(s.pose.tracking && s.intersections == 1);
  engine.set_stop_target({1, 0});
  // Every intermediate frame is independently ray measured, crossing the cell
  // boundary instead of teleporting to waypoint centers.
  for (int i = 1; i <= 40; ++i, t += 50000) {
    const float x = .1f + .005f * i;
    s = engine.update(observation(config.variants[0], x, .1f, 0, t), t);
    if (s.pose.tracking) {
      assert(std::fabs(s.pose.x_m - x) < 1e-5f);
      assert(s.stop.valid);
    }
  }
  assert(s.pose.tracking && s.pose.grid_x == 1 && s.intersections == 2);
  assert(s.stop.should_stop && s.stop.distance_m < 1e-5f);
  assert(s.stop.speed_m_s > .09f && s.stop.speed_m_s < .11f);
  assert(s.stop.allowance_m > .012f);
  // Revisit the first intersection: count a physical return once, then
  // keep the ring bounded across repeated traversals.
  for (int leg = 0; leg < 18; ++leg) {
    for (int i = 1; i <= 40; ++i, t += 50000) {
      const float x = leg % 2 == 0 ? .3f - .005f * i : .1f + .005f * i;
      s = engine.update(observation(config.variants[0], x, .1f, 0, t), t);
    }
    assert(s.pose.tracking);
  }
  assert(s.intersections == 20 && s.history_size == 16);
  assert((s.last_intersection.cell == loc::Cell{1, 0}));
  const auto epoch = s.pose.timestamp_us;
  const auto visits = s.intersections;
  auto duplicate = observation(config.variants[0], .3f, .1f, 0, epoch);
  s = engine.update(duplicate, epoch + 10000);
  assert(s.pose.timestamp_us == epoch && s.intersections == visits);
  s = engine.update(duplicate, epoch + 200000);
  assert(!s.pose.tracking && !s.stop.valid);
  // A plausible feature at a distant disconnected-in-time cell cannot jump
  // the accepted route history, even after a sensor outage.
  t += 250000;
  s = engine.update(observation(config.variants[0], .9f, .9f, 0, t), t);
  assert(!s.pose.tracking);
  auto f = observation(config.variants[0], .3f, .1f, 0, t + 50000);
  f.range_m[1] = std::numeric_limits<float>::quiet_NaN();
  assert(!engine.update(f, t + 50000).pose.tracking);
}
void ambiguity() {
  auto c = nav::default_resolver_config();
  for (unsigned v = 0; v < 4; ++v) {
    c.variants[v] = arena();
    c.variants[v].kind = static_cast<loc::MapKind>(v);
  }
  nav::FeatureNavigation e(c);
  for (int k = 0; k < 8; ++k) {
    auto t = 1000000 + k * 50000;
    auto s = e.update(observation(c.variants[0], .3f, .5f, 0, t), t);
    assert(s.resolution.status == nav::ResolveStatus::AMBIGUOUS &&
           !s.pose.tracking);
  }
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  nav::FeatureNavigation skew(c, cfg);
  auto f = observation(c.variants[0], .3f, .5f, 0, 1000000);
  f.tof_us[2] -= 100000;
  assert(!skew.update(f, 1000000).pose.tracking);
  f = observation(c.variants[0], .3f, .5f, 0, 1000000);
  f.magnetic_clean = false;
  assert(!skew.update(f, 1000000).pose.tracking);
  cfg.sigma_m = 0;
  nav::FeatureNavigation invalid(c, cfg);
  assert(invalid.update(f, 1000000).resolution.status ==
         nav::ResolveStatus::INVALID_CONFIG);
}
void sequence_resolution() {
  auto c = nav::default_resolver_config();
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  unsigned resolved_from_history = 0;
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x)
      for (auto d : {loc::Cell{1, 0}, loc::Cell{0, 1}, loc::Cell{-1, 0},
                     loc::Cell{0, -1}}) {
        loc::Cell destination{x + d.x, y + d.y};
        if (!c.variants[0].can_step({x, y}, destination))
          continue;
        float yaw = std::atan2(float(d.y), float(d.x));
        nav::FeatureNavigation tracked(c, cfg), instant(c, cfg);
        nav::FeatureSnapshot a{}, b{};
        int64_t t = 1000000;
        for (int k = 0; k < 3; ++k, t += 50000)
          a = tracked.update(observation(c.variants[0], (x + .5f) * .2f,
                                         (y + .5f) * .2f, yaw, t),
                             t);
        for (int k = 1; k <= 40; ++k, t += 50000) {
          float px = (x + .5f) * .2f + d.x * .005f * k,
                py = (y + .5f) * .2f + d.y * .005f * k;
          a = tracked.update(observation(c.variants[0], px, py, yaw, t), t);
          if (a.pose.tracking)
            assert(std::hypot(a.pose.x_m - px, a.pose.y_m - py) < 1e-4f);
        }
        for (int k = 0; k < 3; ++k, t += 50000) {
          auto f = observation(c.variants[0], (destination.x + .5f) * .2f,
                               (destination.y + .5f) * .2f, yaw, t);
          a = tracked.update(f, t);
          b = instant.update(f, t);
        }
        if (a.pose.tracking &&
            b.resolution.status == nav::ResolveStatus::AMBIGUOUS)
          ++resolved_from_history;
      }
  assert(resolved_from_history > 0);
  std::printf(
      "%u Q1 traversals resolve an aliased endpoint using feature history\n",
      resolved_from_history);
}
void firmware_map() {
  auto c = nav::default_resolver_config();
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  // Verify all geometrically observable centers; repeated geometry is allowed
  // to remain ambiguous, but a confident result must never name another cell.
  unsigned resolved = 0;
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x)
      for (int h = 0; h < 4; ++h) {
        nav::FeatureNavigation e(c, cfg);
        nav::FeatureSnapshot s{};
        for (int k = 0; k < 3; ++k) {
          auto t = 1000000 + k * 50000;
          s = e.update(observation(c.variants[0], (x + .5f) * .2f,
                                   (y + .5f) * .2f, h * loc::kPi / 2, t),
                       t);
        }
        if (s.pose.tracking) {
          assert(s.pose.grid_x == x && s.pose.grid_y == y);
          ++resolved;
        }
      }
  assert(resolved > 0);
  std::printf(
      "Firmware Q1: %u/100 center/headings uniquely observable; others held\n",
      resolved);
}
} // namespace
int main() {
  tracking();
  ambiguity();
  sequence_resolution();
  firmware_map();
  std::puts("PASS runtime feature ray fit, transitions, history, stop estimate "
            "and ambiguity");
}
