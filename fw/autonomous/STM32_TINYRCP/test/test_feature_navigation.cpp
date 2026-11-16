// SPDX-License-Identifier: GPL-3.0-only

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
  return f; // Deliberately no color or external pose injection.
}
struct HistoricalPose {
  float x, y;
};
// Historical range captures used edge-midpoint side sensors. Moving the
// platform back along its forward axis keeps both captured side-ray origins
// fixed. V2 front readings are recast from its actual relocated origin.
HistoricalPose historical_pose(float x, float y, float yaw) {
  const float offset = robot::geometry::side_tof_x_body_m;
  return {x - offset * std::cos(yaw), y - offset * std::sin(yaw)};
}
nav::BootstrapFrame historical_observation(const loc::MazeMap &map, float x,
                                           float y, float yaw, int64_t t) {
  const auto p = historical_pose(x, y, yaw);
  return observation(map, p.x, p.y, yaw, t);
}
loc::MazeMap arena() {
  auto m = loc::MazeMap::q1();
  for (int r = 0; r < 5; ++r)
    for (int c = 0; c < 5; ++c)
      m.walls[r][c] = (r == 0 ? loc::NORTH : 0) | (r == 4 ? loc::SOUTH : 0) |
                      (c == 0 ? loc::WEST : 0) | (c == 4 ? loc::EAST : 0);
  return m;
}
void full_ranges_reacquire_after_partial_prior() {
  auto maps = nav::default_resolver_config();
  maps.start_count = 1;
  maps.starts[0].cell = {4, 3};
  maps.variants[0] = arena();
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  nav::FeatureNavigation engine(maps, cfg);
  constexpr float x = .8927f, yaw = -1.4481f;
  int64_t now = 1000000;
  nav::FeatureSnapshot result;
  for (int i = 0; i < 3; ++i, now += 50000)
    result = engine.update(observation(maps.variants[0], x, .7626f, -loc::kPi / 2, now), now);
  assert(result.pose.tracking);
  for (int i = 0; i < 3; ++i, now += 50000)
    result = engine.update(observation(maps.variants[0], x, .7626f, yaw, now), now);
  // Move through the genuinely ambiguous arena-corner interval. Once the
  // front sees a reliable second axis, fresh wall returns must recover the
  // coordinate rather than freezing it forever at the old partial fit.
  for (int i = 1; i <= 90; ++i, now += 50000)
    result = engine.update(observation(maps.variants[0], x, .7626f - .001f * i, yaw, now), now);
  assert(result.pose.tracking && result.excluded_ray_mask == 0);
  assert(std::hypot(result.pose.x_m - x, result.pose.y_m - .6726f) < .002f);
}
void tracking() {
  auto config = nav::default_resolver_config();
  // Explicit open-arena fixture, independent of the supplied-map POI list.
  config.start_count = 1;
  config.starts[0].cell = {0, 0};
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
void pivot_observability_and_gyro_ramp() {
  auto config = nav::default_resolver_config();
  config.start_count = 1;
  config.starts[0].cell = {0, 0};
  nav::FeatureConfig settings;
  settings.variant_mask = 1;
  nav::FeatureNavigation engine(config, settings);
  int64_t t = 1000000;
  nav::FeatureSnapshot s{};
  for (int k = 0; k < 3; ++k, t += 50000)
    s = engine.update(observation(config.variants[0], .1f, .1f, 0, t), t);
  assert(s.pose.tracking);
  // All three returns observe X during this stationary pivot. Repetition
  // cannot manufacture a Y coordinate or fresh tracking evidence.
  for (int k = 0; k < 50; ++k, t += 50000) {
    auto f = observation(config.variants[0], .1f, .1f, loc::deg2rad(55), t);
    s = engine.update(f, t);
    assert(!s.pose.tracking && !s.contradiction_latched);
    assert(std::fabs(s.pose.y_m - .1f) < 1e-4f);
  }
  for (int k = 0; k < 3; ++k, t += 50000)
    s = engine.update(
        observation(config.variants[0], .1f, .1f, loc::kPi / 2, t), t);
  assert(s.pose.tracking && std::fabs(s.pose.y_m - .1f) < 1e-4f);

  nav::FeatureNavigation heading(config, settings);
  t = 1000000;
  auto f = observation(config.variants[0], .1f, .1f, 0, t);
  heading.update(f, t);
  // Linear angular acceleration: w=2*t, yaw=t^2. MAG is unavailable under
  // motor drive, so integration alone must preserve the analytic heading.
  for (int k = 1; k <= 20; ++k) {
    const float seconds = k * .05f;
    t += 50000;
    f = observation(config.variants[0], .1f, .1f, seconds * seconds, t);
    f.magnetic_clean = false;
    f.imu.gz_rps = 2 * seconds;
    s = heading.update(f, t);
    assert(s.heading.valid &&
           std::fabs(s.heading.yaw_rad - seconds * seconds) < 1e-5f);
  }
}
void ambiguity() {
  auto c = nav::default_resolver_config();
  for (unsigned v = 0; v < 4; ++v) {
    c.variants[v] = arena();
    c.variants[v].kind = static_cast<loc::MapKind>(v);
  }
  c.start_count = 1;
  c.starts[0].cell = {2, 2};
  nav::FeatureNavigation e(c);
  for (int k = 0; k < 8; ++k) {
    auto t = 1000000 + k * 50000;
    auto s = e.update(observation(c.variants[0], .5f, .5f, 0, t), t);
    assert(s.resolution.status == nav::ResolveStatus::AMBIGUOUS &&
           !s.pose.tracking && !s.contradiction_latched &&
           s.contradiction_frames == 0);
  }
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  nav::FeatureNavigation skew(c, cfg);
  auto f = observation(c.variants[0], .5f, .5f, 0, 1000000);
  f.tof_us[2] -= 100000;
  assert(!skew.update(f, 1000000).pose.tracking);
  f = observation(c.variants[0], .5f, .5f, 0, 1000000);
  f.magnetic_clean = false;
  assert(!skew.update(f, 1000000).pose.tracking);
  cfg.sigma_m = 0;
  nav::FeatureNavigation invalid(c, cfg);
  assert(invalid.update(f, 1000000).resolution.status ==
         nav::ResolveStatus::INVALID_CONFIG);
  cfg.sigma_m = .1001f;
  nav::FeatureNavigation oversized(c, cfg);
  assert(oversized.update(f, 1000000).resolution.status ==
         nav::ResolveStatus::INVALID_CONFIG);
}
void contradictions() {
  auto config = nav::default_resolver_config();
  // Explicit open-arena fixture, independent of the supplied-map POI list.
  config.start_count = 1;
  config.starts[0].cell = {0, 0};
  config.variants[0] = arena();
  nav::FeatureConfig settings;
  settings.variant_mask = 1;
  nav::FeatureNavigation engine(config, settings);
  int64_t t = 1000000;
  auto frame = [&](bool mismatch) {
    auto f = observation(config.variants[0], .1f, .1f, 0, t);
    if (mismatch)
      for (auto &range : f.range_m)
        range = .05f; // Fresh, in range, but inconsistent with the wall map.
    return f;
  };
  nav::FeatureSnapshot s{};
  // No accepted history yet: failed startup matching must remain stationary,
  // rather than arm reactive motion.
  for (int i = 0; i < 5; ++i, t += 50000) {
    s = engine.update(frame(true), t);
    assert(s.resolution.status == nav::ResolveStatus::NO_CANDIDATE &&
           !s.contradiction_latched && s.contradiction_frames == 0);
  }
  for (int i = 0; i < 3; ++i, t += 50000)
    s = engine.update(frame(false), t);
  assert(s.pose.tracking);
  auto bad = frame(true);
  s = engine.update(bad, t);
  assert(s.contradiction_frames == 1 && !s.contradiction_latched);
  for (int i = 1; i <= 4; ++i)
    s = engine.update(bad, t + i * 5000);
  assert(s.contradiction_frames == 1 && !s.contradiction_latched);
  // A valid match resets transient contradiction evidence.
  t += 50000;
  s = engine.update(frame(false), t);
  assert(s.pose.tracking && s.contradiction_frames == 0);
  t += 50000;
  s = engine.update(frame(true), t);
  assert(s.contradiction_frames == 1);
  // Stale/skewed inputs are pauses, not further contradiction evidence.
  t += 50000;
  auto skew = frame(true);
  skew.tof_us[2] -= 100000;
  s = engine.update(skew, t);
  assert(s.resolution.status == nav::ResolveStatus::WAIT_SENSORS &&
         s.contradiction_frames == 0 && !s.contradiction_latched);
  t += 50000;
  s = engine.update(frame(true), t);
  assert(s.contradiction_frames == 1);
  t += 250000; // A gap also starts a fresh streak.
  s = engine.update(frame(true), t);
  assert(s.contradiction_frames == 1 && !s.contradiction_latched);
  for (unsigned count = 2; count <= settings.contradiction_limit; ++count) {
    t += 50000;
    s = engine.update(frame(true), t);
    assert(s.contradiction_frames == count && !s.pose.tracking &&
           !s.stop.valid);
    assert(s.contradiction_latched == (count == settings.contradiction_limit));
  }
  assert(s.pose.health == loc::FusionHealth::CONTRADICTING);
  const auto trip = s.contradiction_us;
  t += 50000;
  s = engine.update(frame(false), t);
  // A recovered independent pose may support the tape gate, but cannot erase
  // the diagnostic latch or switch the path task back from RHR to A*.
  assert(s.pose.tracking && s.pose.health == loc::FusionHealth::TRACKING &&
         s.contradiction_latched && s.contradiction_us == trip &&
         s.contradiction_frames == settings.contradiction_limit);
  settings.contradiction_limit = 1;
  nav::FeatureNavigation invalid(config, settings);
  assert(invalid.update(frame(false), t).resolution.status ==
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
        auto fixture = c;
        fixture.start_count = 1;
        fixture.starts[0].cell = {x, y};
        nav::FeatureNavigation tracked(fixture, cfg), instant(fixture, cfg);
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
        if (a.pose.tracking && !b.pose.tracking)
          ++resolved_from_history;
      }
  assert(resolved_from_history > 0);
  std::printf(
      "%u Q1 traversals track beyond configured starts using feature history\n",
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
        auto fixture = c;
        fixture.start_count = 1;
        fixture.starts[0].cell = {x, y};
        nav::FeatureNavigation e(fixture, cfg);
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
void startup_permutations() {
  const auto maps = nav::default_resolver_config();
  unsigned resolved[2][4]{};
  // The same L/F/R pattern occurs in every rotated arena. Only the absolute
  // heading changes; it must select the corresponding variant and world cell.
  for (unsigned deviation = 0; deviation < 2; ++deviation)
    for (unsigned heading = 0; heading < 4; ++heading)
      for (unsigned entrance = 0; entrance < maps.start_count; ++entrance) {
        const int x = maps.starts[entrance].cell.x;
        const int y = maps.starts[entrance].cell.y;
        const float local_yaw = heading * loc::kPi / 2 + deviation * .2f;
        const auto pattern = observation(maps.variants[0], (x + .5f) * .2f,
                                         (y + .5f) * .2f, local_yaw, 1000000);
        for (unsigned v = 0; v < 4; ++v) {
          nav::FeatureConfig settings;
          // Continuous yaw may make entrance geometry unobservable. Verify
          // any confident match, and retain real ambiguities without motion.
          if (deviation)
            settings.variant_mask = 1U << v;
          nav::FeatureNavigation engine(maps, settings), blind(maps);
          nav::FeatureSnapshot s{};
          const float yaw = local_yaw + v * loc::kPi / 2;
          for (int k = 0; k < 3; ++k) {
            const int64_t t = 1000000 + k * 50000;
            auto f = pattern;
            f.imu_us = f.mag_us = t;
            f.mag = {45 * std::cos(yaw), -45 * std::sin(yaw), 0};
            for (auto &epoch : f.tof_us)
              epoch = t;
            s = engine.update(f, t);
            const auto unrestricted = blind.update(f, t);
            if (unrestricted.pose.tracking) {
              const auto world = loc::rotate_cell({x, y}, v);
              assert(unsigned(unrestricted.resolution.kind) == v);
              assert(unrestricted.pose.grid_x == world.x &&
                     unrestricted.pose.grid_y == world.y);
            }
            if (k < 2)
              assert(!s.pose.tracking);
          }
          if (!s.pose.tracking)
            continue; // Repeated wall signatures must remain unresolved.
          const auto world = loc::rotate_cell({x, y}, v);
          assert(unsigned(s.resolution.kind) == v);
          assert(s.pose.grid_x == world.x && s.pose.grid_y == world.y);
          assert(std::hypot(s.pose.x_m - (world.x + .5f) * .2f,
                            s.pose.y_m - (world.y + .5f) * .2f) < 1e-5f);
          assert(std::fabs(loc::wrap_pi(s.pose.yaw_rad - yaw)) < 1e-5f);
          ++resolved[deviation][v];

          // A single physically impossible wall distance cannot seed a map,
          // even when the other beams and the MAG heading still agree.
          nav::FeatureNavigation mismatch(maps);
          auto f = pattern;
          f.mag = {45 * std::cos(yaw), -45 * std::sin(yaw), 0};
          // Inside sensor limits, beyond the mapped wall for these headings.
          f.range_m[1] = 1.3f;
          s = mismatch.update(f, 1000000);
          assert(s.resolution.status == nav::ResolveStatus::NO_CANDIDATE &&
                 !s.pose.tracking);
        }
      }
  for (unsigned deviation = 0; deviation < 2; ++deviation)
    for (unsigned v = 0; v < 4; ++v) {
      if (!deviation)
        assert(resolved[deviation][v] > 0);
      std::printf("Startup Q%u, mask %u, yaw offset %.1f: %u unique "
                  "range-pattern/heading matches\n",
                  v + 1, deviation ? 1U << v : 15, deviation * .2f,
                  resolved[deviation][v]);
    }
}
void startup_entrances_and_random_facing() {
  const auto maps = nav::default_resolver_config();
  for (unsigned v = 0; v < 4; ++v)
    for (unsigned entrance = 0; entrance < maps.start_count; ++entrance)
      for (unsigned h = 0; h < 4; ++h) {
        const auto start = loc::rotate_cell(maps.starts[entrance].cell, v);
        nav::FeatureConfig cfg;
        cfg.variant_mask = 1U << v;
        nav::FeatureNavigation engine(maps, cfg);
        nav::FeatureSnapshot result{};
        for (int k = 0; k < 3; ++k) {
          const int64_t t = 1000000 + k * 50000;
          result = engine.update(observation(maps.variants[v],
                                             (start.x + .5f) * loc::kCellM,
                                             (start.y + .5f) * loc::kCellM,
                                             (h + v) * loc::kPi / 2, t),
                                 t);
          for (unsigned variant = 0; variant < 4; ++variant) {
            uint32_t allowed = 0;
            for (unsigned i = 0; i < maps.start_count; ++i) {
              const auto cell = loc::rotate_cell(maps.starts[i].cell, variant);
              allowed |= uint32_t{1} << (cell.y * 5 + cell.x);
            }
            assert(!(result.candidate_cells[variant] & ~allowed));
          }
        }
        if (result.pose.tracking) {
          assert(result.resolution.kind == maps.variants[v].kind);
          assert(result.pose.grid_x == start.x &&
                 result.pose.grid_y == start.y);
        } else {
          // Repeated signatures among the configured starts must remain
          // unresolved; their heading and multiplicity depend on the POIs.
          assert(result.resolution.status == nav::ResolveStatus::AMBIGUOUS);
          assert(result.candidates >= 2 &&
                 result.candidates <= maps.start_count);
        }
      }
  auto bad = maps;
  bad.start_count = 0;
  auto f = observation(maps.variants[0], .9f, .1f, 0, 1000000);
  assert(nav::FeatureNavigation(bad).update(f, 1000000).resolution.status ==
         nav::ResolveStatus::INVALID_CONFIG);
  bad = maps;
  bad.start_count = 9;
  assert(nav::FeatureNavigation(bad).update(f, 1000000).resolution.status ==
         nav::ResolveStatus::INVALID_CONFIG);
  bad = maps;
  bad.starts[0].cell = {-1, 0};
  assert(nav::FeatureNavigation(bad).update(f, 1000000).resolution.status ==
         nav::ResolveStatus::INVALID_CONFIG);
}
void startup_rotated_boundaries() {
  auto maps = nav::default_resolver_config();
  const auto reference = arena();
  maps.start_count = 2;
  // Both sides of the exact boundary are legal synthetic starts. Rotation
  // changes which reference side maps to the inclusive world cell edge.
  maps.starts[0].cell = {1, 1};
  maps.starts[1].cell = {2, 1};
  for (unsigned v = 0; v < 4; ++v) {
    maps.variants[v] = reference.rotated(v, static_cast<loc::MapKind>(v));
    nav::FeatureConfig settings;
    settings.variant_mask = 1U << v;
    nav::FeatureNavigation engine(maps, settings);
    float x = .4f, y = .3f; // Exactly on an open cell boundary.
    for (unsigned turn = 0; turn < v; ++turn) {
      const float next_x = 1 - y;
      y = x;
      x = next_x;
    }
    nav::FeatureSnapshot s{};
    for (int k = 0; k < 3; ++k) {
      const int64_t t = 1000000 + k * 50000;
      s = engine.update(
          observation(maps.variants[v], x, y, v * loc::kPi / 2, t), t);
    }
    assert(s.pose.tracking && unsigned(s.resolution.kind) == v);
    assert(s.pose.grid_x == int(s.pose.x_m / loc::kCellM));
    assert(s.pose.grid_y == int(s.pose.y_m / loc::kCellM));
    assert(std::hypot(s.pose.x_m - x, s.pose.y_m - y) < 1e-5f);
  }
}
void tilted_endpoint_with_two_orthogonal_walls() {
  auto maps = nav::default_resolver_config();
  maps.start_count = 1;
  maps.starts[0].cell = {2,
                         3}; // Explicit synthetic entrance for the ray fixture.
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  nav::FeatureNavigation engine(maps, cfg);
  int64_t t = 1000000;
  constexpr float yaw = 1.62479f;
  // Keep the captured side-ray origins fixed after moving both side ToFs
  // forward: translate the synthetic platform pose backward by that mount
  // offset. The front sensor has not moved and uses its new geometric range.
  const float offset = robot::geometry::side_tof_x_body_m;
  const float x = .506128f - offset * std::cos(yaw);
  const float y_offset = offset * std::sin(yaw);
  nav::FeatureSnapshot result{};
  for (int k = 0; k < 3; ++k, t += 50000)
    result = engine.update(
        observation(maps.variants[0], x, .7f - y_offset, yaw, t), t);
  assert(result.pose.tracking);
  for (int k = 1; k <= 32; ++k, t += 50000) {
    const float y = .7f + .084f * k / 32 - y_offset;
    result = engine.update(observation(maps.variants[0], x, y, yaw, t), t);
    assert(result.pose.tracking);
  }
  const float y = .787221f - y_offset;
  auto f = observation(maps.variants[0], x, y, yaw, t);
  // Exact independent simulator readings captured at the five-second stall.
  // L is a lone X endpoint, F/R hit distinct reliable Y wall segments.
  f.range_m[0] = .0363497f;
  // Front reading is geometric for v2; side endpoint captures stay fixed.
  f.range_m[1] = observation(maps.variants[0], x, y, yaw, t).range_m[1];
  f.range_m[2] = .176343f;
  result = engine.update(f, t);
  assert(result.pose.tracking);
  assert(std::hypot(result.pose.x_m - x, result.pose.y_m - y) < .005f);
  t += 50000;
  f = observation(maps.variants[0], x, y, yaw, t);
  f.range_m[1] += .06f; // Orthogonal walls no longer independently agree.
  result = engine.update(f, t);
  assert(!result.pose.tracking && !result.stop.valid);
}
void endpoint_association_does_not_switch_to_distant_wall() {
  auto maps = nav::default_resolver_config();
  maps.start_count = 1;
  maps.starts[0].cell = {0, 4}; // Explicit final-approach geometry fixture.
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  nav::FeatureNavigation engine(maps, cfg);
  int64_t t = 1000000;
  constexpr float x = .105734f, yaw = -1.631303f;
  const auto expected = historical_pose(.107786f, .828472f, yaw);
  nav::FeatureSnapshot result{};
  for (int k = 0; k < 3; ++k, t += 50000)
    result = engine.update(
        historical_observation(maps.variants[0], x, .9f, yaw, t), t);
  assert(result.pose.tracking);
  for (int k = 1; k <= 32; ++k, t += 50000) {
    const float y = .9f - .065913f * k / 32;
    result = engine.update(
        historical_observation(maps.variants[0], x, y, yaw, t), t);
    assert(result.pose.tracking);
  }
  // The left ray is near a finite wall endpoint. A noisy first refit can
  // make it switch from x=.2 to x=1 or y=0, despite the actual return below.
  // Fresh F/R independently observe both axes and must retain the valid pose.
  for (int k = 0; k < 100; ++k, t += 50000) {
    auto f = historical_observation(maps.variants[0], x, .834087f, yaw, t);
    f.range_m[0] = .444610f;
    f.range_m[2] = .040808f;
    result = engine.update(f, t);
    assert(result.pose.tracking && !result.contradiction_latched);
    assert(result.excluded_ray_mask == 0);
    assert(std::fabs(result.pose.x_m - expected.x) < .002f);
    assert(std::fabs(result.pose.y_m - expected.y) < .002f);
    for (int i = 0; i < 3; ++i) {
      const auto hit =
          loc::trace_maze_ray(maps.variants[0], result.pose.x_m,
                              result.pose.y_m, yaw, loc::cfg::lidar_mounts[i]);
      assert(std::fabs(hit.range - f.range_m[i]) <= cfg.sigma_m);
    }
  }
  for (int k = 1; k <= 24; ++k, t += 50000) {
    const float y = .828472f - .058472f * k / 24;
    result = engine.update(
        historical_observation(maps.variants[0], .107786f, y, yaw, t), t);
  }
  assert(result.pose.tracking && !result.contradiction_latched);
  assert(result.excluded_ray_mask ==
         0); // Stable measured wall return restored.
}
void grazing_association_keeps_independent_position() {
  auto maps = nav::default_resolver_config();
  maps.start_count = 1;
  maps.starts[0].cell = {4, 3}; // Explicit runtime geometry fixture.
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  nav::FeatureNavigation engine(maps, cfg);
  int64_t t = 1000000;
  constexpr float yaw = 3.092106f;
  const auto expected = historical_pose(.8059198f, .6965873f, yaw);
  nav::FeatureSnapshot result{};
  for (int k = 0; k < 3; ++k, t += 50000)
    result = engine.update(
        historical_observation(maps.variants[0], .9f, .7f, yaw, t), t);
  assert(result.pose.tracking);
  for (int k = 1; k <= 32; ++k, t += 50000) {
    const float x = .9f - .0940802f * k / 32;
    const float y = .7f - .0034127f * k / 32;
    result = engine.update(
        historical_observation(maps.variants[0], x, y, yaw, t), t);
    assert(result.pose.tracking);
  }
  // Captured H3 frame: L grazes x=.8 and retains one wrong wall ID through
  // every refit. F/R independently observe X/Y and agree with measured history.
  for (int k = 0; k < 20; ++k, t += 50000) {
    auto f =
        historical_observation(maps.variants[0], .8059198f, .6965873f, yaw, t);
    f.range_m[0] = .227590f;
    f.range_m[2] = .038164f;
    result = engine.update(f, t);
    assert(result.pose.tracking && !result.contradiction_latched);
    // The full-frame seed search finds the correct wall side immediately;
    // every channel agrees, so the two-ray fallback is unnecessary.
    assert(result.excluded_ray_mask == 0);
    for (int i = 0; i < 3; ++i) {
      const auto hit =
          loc::trace_maze_ray(maps.variants[0], result.pose.x_m,
                              result.pose.y_m, yaw, loc::cfg::lidar_mounts[i]);
      assert(std::fabs(hit.range - f.range_m[i]) <= cfg.sigma_m);
    }
    // These retained noisy side readings no longer have the original front
    // sensor's conditioning. Bound the fit by the configured range sigma.
    assert(std::hypot(result.pose.x_m - expected.x,
                      result.pose.y_m - expected.y) < cfg.sigma_m);
  }
  for (int i = 0; i < 3; ++i, t += 50000)
    result = engine.update(historical_observation(maps.variants[0], .8059198f, .6965873f, yaw, t), t);
  assert(result.pose.tracking);
  assert(std::hypot(result.pose.x_m - expected.x, result.pose.y_m - expected.y) < .002f);
  auto bad = historical_observation(maps.variants[0], .8022f, .6948f, yaw, t);
  bad.range_m[1] += .1f;
  result = engine.update(bad, t);
  assert(!result.pose.tracking && !result.stop.valid);
}
void grazing_association_recovers_after_surface_approach() {
  auto maps = nav::default_resolver_config();
  maps.start_count = 1;
  maps.starts[0].cell = {2, 3}; // Explicit runtime geometry fixture.
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  nav::FeatureNavigation engine(maps, cfg);
  int64_t t = 1000000;
  constexpr float x = .5034505f, yaw = 1.62444937f;
  const auto expected = historical_pose(x, .7926213f, yaw);
  nav::FeatureSnapshot result{};
  for (int k = 0; k < 3; ++k, t += 50000)
    result = engine.update(
        historical_observation(maps.variants[0], x, .7f, yaw, t), t);
  assert(result.pose.tracking);
  for (int k = 1; k <= 32; ++k, t += 50000) {
    const float y = .7f + .0926213f * k / 32;
    result = engine.update(
        historical_observation(maps.variants[0], x, y, yaw, t), t);
    assert(result.pose.tracking);
  }
  // Captured whiteboard-plant frame: R grazes y=.8 while L is endpoint-near.
  // A first refit makes L unambiguous, allowing F/L to reject the grazing R
  // association. The corrected position exposes R's actual x=1 return.
  for (int k = 0; k < 20; ++k, t += 50000) {
    auto f = historical_observation(maps.variants[0], x, .7926213f, yaw, t);
    f.range_m[0] = .036267843f;
    f.range_m[2] = .42825228f;
    result = engine.update(f, t);
    assert(result.pose.tracking && !result.contradiction_latched);
    assert(std::hypot(result.pose.x_m - expected.x,
                      result.pose.y_m - expected.y) < cfg.sigma_m);
  }
  // With v2's actual front ray and no capture noise, recover the physical pose.
  for (int i = 0; i < 3; ++i, t += 50000)
    result = engine.update(historical_observation(maps.variants[0], x, .7926213f, yaw, t), t);
  assert(result.pose.tracking);
  assert(std::hypot(result.pose.x_m - expected.x, result.pose.y_m - expected.y) < .002f);
}
void noisy_cell_boundary_keeps_visible_walls() {
  auto maps = nav::default_resolver_config();
  maps.start_count = 1;
  maps.starts[0].cell = {3, 1};
  nav::FeatureConfig cfg;
  cfg.variant_mask = 1;
  nav::FeatureNavigation engine(maps, cfg);
  int64_t t = 1000000;
  constexpr float y = .295336f, yaw = 3.080970f;
  const auto expected = historical_pose(.607914f, y, yaw);
  nav::FeatureSnapshot result{};
  for (int k = 0; k < 3; ++k, t += 50000)
    result = engine.update(
        historical_observation(maps.variants[0], .7f, y, yaw, t), t);
  assert(result.pose.tracking);
  for (int k = 1; k <= 32; ++k, t += 50000) {
    const float x = .7f - .092086f * k / 32;
    result = engine.update(
        historical_observation(maps.variants[0], x, y, yaw, t), t);
    assert(result.pose.tracking);
  }
  for (int k = 0; k < 20; ++k, t += 50000) {
    auto f = historical_observation(maps.variants[0], .607914f, y, yaw, t);
    f.range_m[0] = .033748f;
    f.range_m[2] = .039403f;
    result = engine.update(f, t);
    assert(result.pose.tracking && !result.contradiction_latched);
    assert((result.pose.grid_x == 2 || result.pose.grid_x == 3) &&
           result.pose.grid_y == 1);
    assert(std::fabs(result.pose.x_m - expected.x) < cfg.sigma_m);
    assert(std::fabs(result.pose.y_m - expected.y) < .01f);
  }
  // With v2's actual front ray and no capture noise, recover the physical pose.
  for (int i = 0; i < 3; ++i, t += 50000)
    result = engine.update(historical_observation(maps.variants[0], .607914f, y, yaw, t), t);
  assert(result.pose.tracking);
  assert(std::hypot(result.pose.x_m - expected.x, result.pose.y_m - expected.y) < .002f);
}
void pivot_ray_recovers_a_missed_nearer_endpoint() {
  auto maps = nav::default_resolver_config();
  // Historical endpoint geometry; its old Q1 POI is not a production start.
  maps.start_count = 1;
  maps.starts[0].cell = {0, 0};
  nav::FeatureConfig cfg;
  cfg.variant_mask = 8;
  nav::FeatureNavigation engine(maps, cfg);
  int64_t t = 1000000;
  const auto expected = historical_pose(.104440f, .905957f, 1.241534f);
  nav::FeatureSnapshot result{};
  for (int k = 0; k < 3; ++k, t += 50000)
    result = engine.update(
        historical_observation(maps.variants[3], .1f, .9f, loc::kPi / 2, t), t);
  assert(result.pose.tracking);
  for (int k = 1; k <= 32; ++k, t += 50000) {
    const float yaw = loc::kPi / 2 - (loc::kPi / 2 - 1.247377f) * k / 32;
    result = engine.update(
        historical_observation(maps.variants[3], .1f + .004440f * k / 32,
                               .9f + .005957f * k / 32, yaw, t),
        t);
    assert(result.pose.tracking);
  }
  // Q4 seed13 pivot: R's measured hit is beside the nearer wall's endpoint,
  // but the noisy prior pose predicts a distant x=.8 wall instead. L/F
  // independently constrain X/Y; retain their fit rather than the wrong R ID.
  for (int k = 0; k < 20; ++k, t += 50000) {
    auto f = historical_observation(maps.variants[3], .104440f, .905957f,
                                    1.241534f, t);
    f.range_m[0] = .037512f;
    f.range_m[2] = .241946f;
    result = engine.update(f, t);
    assert(result.pose.tracking && !result.contradiction_latched);
    assert(std::hypot(result.pose.x_m - expected.x,
                      result.pose.y_m - expected.y) < cfg.sigma_m);
  }
  // With v2's actual front ray and no capture noise, recover the physical pose.
  for (int i = 0; i < 3; ++i, t += 50000)
    result = engine.update(historical_observation(maps.variants[3], .104440f, .905957f, 1.241534f, t), t);
  assert(result.pose.tracking);
  assert(std::hypot(result.pose.x_m - expected.x, result.pose.y_m - expected.y) < .002f);
}
void corridor_opening_tracking() {
  const auto maps = nav::default_resolver_config();
  for (unsigned v = 0; v < 4; ++v) {
    nav::FeatureConfig settings;
    settings.variant_mask = 1U << v;
    nav::FeatureNavigation engine(maps, settings);
    int64_t t = 1000000;
    auto sample = [&](float x, float y, float local_yaw,
                      bool corrupt_side = false) {
      for (unsigned turn = 0; turn < v; ++turn) {
        const float next_x = 1 - y;
        y = x;
        x = next_x;
      }
      nav::FeatureSnapshot s{};
      for (int k = 0; k < 3; ++k, t += 50000) {
        auto f = observation(maps.variants[v], x, y,
                             local_yaw + v * loc::kPi / 2, t);
        if (corrupt_side)
          f.range_m[0] += .06f;
        s = engine.update(f, t);
      }
      return s;
    };
    assert(sample(.9f, .1f, loc::kPi).pose.tracking);
    assert(sample(.9f, .1f, loc::kPi / 2).pose.tracking);
    // Before y=.6 the left wall ends, so its return is endpoint-ambiguous.
    // The front and right walls still independently measure Y and X. A
    // controller that pauses on missing pose must not become stuck here.
    for (int k = 1; k <= 120; ++k) {
      const float y = .1005f + .005f * k;
      const auto s = sample(.9f, y, loc::kPi / 2);
      if (!s.pose.tracking || s.contradiction_latched)
        std::fprintf(stderr, "corridor Q%u y=%.3f status=%u candidates=%u\n",
                     v + 1, y, unsigned(s.resolution.status), s.candidates);
      assert(s.pose.tracking && !s.contradiction_latched);
      float expected_x = .9f, expected_y = y;
      for (unsigned turn = 0; turn < v; ++turn) {
        const float next_x = 1 - expected_y;
        expected_y = expected_x;
        expected_x = next_x;
      }
      assert(std::hypot(s.pose.x_m - expected_x, s.pose.y_m - expected_y) <
             1e-4f);
    }
    assert(sample(.9f, .7005f, loc::kPi).pose.tracking);
    // At the next boundary both side beams approach wall endpoints. They
    // independently agree on Y while the front ray measures X.
    for (int k = 1; k <= 65; ++k) {
      const float x = .9003f - .005f * k;
      const auto s = sample(x, .7005f, loc::kPi);
      if (!s.pose.tracking)
        std::fprintf(stderr, "opposing endpoints Q%u x=%.3f status=%u\n", v + 1,
                     x, unsigned(s.resolution.status));
      assert(s.pose.tracking && !s.contradiction_latched);
    }
    for (int k = 1; k <= 8; ++k)
      assert(sample(.5753f + .005f * k, .7005f, loc::kPi).pose.tracking);
    assert(sample(.6113f, .7005f, loc::kPi).pose.tracking);
    // A lone endpoint beam, or disagreeing opposing endpoints, cannot supply
    // a missing coordinate. The controller must pause on this bad frame.
    // Preserve the original endpoint ray origins after the side sensors move
    // forward along a west-facing platform; center X moves east by 22.5 mm.
    const float endpoint_x = .6113f + robot::geometry::side_tof_x_body_m;
    assert(sample(endpoint_x, .7005f, loc::kPi).pose.tracking);
    const auto bad = sample(endpoint_x, .7005f, loc::kPi, true);
    assert(!bad.pose.tracking && !bad.stop.valid);
  }
  // The runtime recovery must not relax the initial map/pose handshake.
  nav::FeatureConfig settings;
  settings.variant_mask = 1;
  nav::FeatureNavigation startup(maps, settings);
  for (int k = 0; k < 8; ++k) {
    const int64_t t = 1000000 + k * 50000;
    const auto s = startup.update(
        observation(maps.variants[0], .9f, .585f, loc::kPi / 2, t), t);
    assert(!s.pose.tracking);
  }
}
} // namespace
int main() {
  full_ranges_reacquire_after_partial_prior();
  tracking();
  pivot_observability_and_gyro_ramp();
  ambiguity();
  contradictions();
  sequence_resolution();
  firmware_map();
  startup_permutations();
  startup_entrances_and_random_facing();
  startup_rotated_boundaries();
  tilted_endpoint_with_two_orthogonal_walls();
  endpoint_association_does_not_switch_to_distant_wall();
  grazing_association_keeps_independent_position();
  grazing_association_recovers_after_surface_approach();
  noisy_cell_boundary_keeps_visible_walls();
  pivot_ray_recovers_a_missed_nearer_endpoint();
  corridor_opening_tracking();
  std::puts("PASS runtime feature ray fit, transitions, history, stop estimate "
            "and ambiguity");
}
