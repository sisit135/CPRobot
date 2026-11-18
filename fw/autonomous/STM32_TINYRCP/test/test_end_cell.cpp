// SPDX-License-Identifier: GPL-3.0-only

#include "localization/config.hpp"
#include "navigation/end_cell.hpp"
#include "navigation/feature_navigation.hpp"
#include "navigation/map_resolver.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
namespace {
void detector() {
  nav::EndTapeDetector detector({150, 800});
  nav::EndTapeEvent e{};
  loc::RgbcSample raw{};
  int64_t t = 1000000;
  auto sample = [&](unsigned clear, bool saturated = false) {
    raw.clear = clear;
    raw.saturated = saturated;
    raw.sequence++;
    t += 40000;
    return detector.process(raw, t, &e);
  };
  for (int i = 0; i < 5; ++i)
    assert(!sample(50)); // Start on black never creates an entry.
  for (int i = 0; i < 3; ++i)
    assert(!sample(1200));
  assert(!sample(100));
  assert(!sample(100));
  assert(sample(100));
  assert(e.id == 1 && e.first_us < e.confirmed_us);
  assert(!detector.process(raw, t, &e));
  const auto origin = e.first_us;
  for (int i = 0; i < 4; ++i) {
    assert(sample(100));
    assert(e.id == 1 && e.first_us == origin && e.confirmed_us == t);
  }
  for (int i = 0; i < 3; ++i)
    assert(!sample(1200));
  assert(!sample(100));
  assert(!sample(500)); // Hysteresis band resets debounce.
  assert(!sample(100));
  assert(!sample(100));
  assert(sample(100));
  assert(e.id == 2);
  for (int i = 0; i < 3; ++i)
    assert(!sample(1200));
  assert(!sample(100, true));
  for (int i = 0; i < 5; ++i)
    assert(!sample(100));
  for (int i = 0; i < 3; ++i)
    assert(!sample(1200));
  t += 200000;
  assert(!sample(100));
  assert(!sample(100));
  assert(!sample(100)); // Gap disarms.
  nav::EndTapeDetector disabled;
  assert(!disabled.process(raw, t, &e));
}
loc::PoseSnapshot pose(float x, float y, float yaw, int64_t t) {
  loc::PoseSnapshot p{};
  p.x_m = x;
  p.y_m = y;
  p.yaw_rad = yaw;
  p.timestamp_us = t;
  p.tracking = true;
  return p;
}
void gate() {
  const auto m = loc::MazeMap::q1();
  nav::EndCellGate g;
  g.begin({0, 1}, 1000000);
  nav::BootstrapFrame f;
  f.end_tape = {1, 1100000, 1180000};
  auto p = pose(.1f, .18f, loc::kPi / 2, 1180000);
  assert(g.observe(m, f, p, 1180000));
  assert(!g.at_center(p, 1180000));
  p = pose(.1f, .3f, loc::kPi / 2, 1300000);
  assert(g.at_center(p, 1300000));
  p.tracking = false;
  assert(!g.at_center(p, 1300000));
  p.tracking = true;
  assert(!g.at_center(p, 1500001));
  g.begin({0, 1}, 1000000);
  assert(!g.observe(m, f, pose(.1f, .18f, loc::kPi / 2, 1180000),
                    1180000)); // Same ID consumed.
  nav::EndCellGate wrong;
  wrong.begin({0, 1}, 1000000);
  assert(!wrong.observe(m, f, pose(.1f, .18f, 0, 1180000), 1180000));
  nav::EndCellGate far;
  far.begin({0, 1}, 1000000);
  assert(!far.observe(m, f, pose(.5f, .18f, loc::kPi / 2, 1180000), 1180000));
  nav::EndCellGate old;
  old.begin({0, 1}, 1200000);
  assert(!old.observe(m, f, p, 1300000));
  auto arena = m;
  for (int r = 0; r < 5; ++r)
    for (int c = 0; c < 5; ++c)
      arena.walls[r][c] = (r == 0 ? loc::NORTH : 0) |
                          (r == 4 ? loc::SOUTH : 0) | (c == 0 ? loc::WEST : 0) |
                          (c == 4 ? loc::EAST : 0);
  for (int h = 0; h < 4; ++h) {
    nav::EndCellGate side;
    side.begin({2, 2}, 1000000);
    const float yaw = h * loc::kPi / 2;
    assert(side.observe(arena, f,
                        pose(.5f - .12f * std::cos(yaw),
                             .5f - .12f * std::sin(yaw), yaw, 1180000),
                        1180000));
    assert(side.at_center(pose(.5f, .5f, yaw, 1300000), 1300000));
  }
  nav::EndCellGate future;
  future.begin({0, 1}, 1000000);
  assert(!future.observe(m, f, p, 1100000));
  nav::EndCellGate pending;
  pending.begin({0, 1}, 1000000);
  auto unavailable = pose(.1f, .18f, loc::kPi / 2, 1180000);
  unavailable.tracking = false;
  assert(!pending.observe(m, f, unavailable, 1180000));
  assert(
      pending.observe(m, f, pose(.1f, .18f, loc::kPi / 2, 1230000), 1230000));
  nav::EndCellGate expired;
  expired.begin({0, 1}, 1000000);
  assert(!expired.observe(m, f, unavailable, 1180000));
  assert(
      !expired.observe(m, f, pose(.1f, .18f, loc::kPi / 2, 1340000), 1340000));
  // A valid pose with the wrong geometry consumes the cue immediately.
  assert(!far.observe(m, f, pose(.1f, .18f, loc::kPi / 2, 1230000), 1230000));
}
void pose_only_goal() {
  nav::EndCellGate gate;
  gate.begin({2, 2}, 1000000);
  auto p = pose(.5f, .5f, .7f, 1100000);
  assert(!gate.latched() && !gate.at_center(p, 1100000));
  assert(gate.pose_at_center(p, 1100000)); // No tape or entrance heading.
  p.x_m += .013f;
  assert(!gate.pose_at_center(p, 1100000));
  p.x_m = .5f;
  p.tracking = false;
  assert(!gate.pose_at_center(p, 1100000));
  p.tracking = true;
  assert(!gate.pose_at_center(p, 1250001)); // Stale independent pose.
  assert(!gate.pose_at_center(p, 1099999)); // Future pose.
  p.timestamp_us = 999999;
  assert(!gate.pose_at_center(p, 1100000)); // Before this goal was requested.
  gate.begin({}, 1000000);
  p.timestamp_us = 1100000;
  assert(!gate.pose_at_center(p, 1100000)); // No valid goal.
}
void continuous_tape_pose_recovery() {
  nav::EndTapeDetector detector({150, 800});
  nav::EndCellGate gate;
  gate.begin({0, 1}, 1000000);
  nav::BootstrapFrame frame{};
  loc::RgbcSample raw{};
  auto sample = [&](unsigned clear, int64_t stamp) {
    raw.clear = clear;
    ++raw.sequence;
    return detector.process(raw, stamp, &frame.end_tape);
  };
  for (int i = 0; i < 3; ++i)
    assert(!sample(1200, 1040000 + i * 40000));
  for (int i = 0; i < 2; ++i)
    assert(!sample(100, 1160000 + i * 40000));
  assert(sample(100, 1240000));
  auto p = pose(.1f, .18f, loc::kPi / 2, 1240000);
  p.tracking = false;
  assert(!gate.observe(loc::MazeMap::q1(), frame, p, 1240000));
  for (int i = 1; i <= 10; ++i) {
    const int64_t now = 1240000 + i * 40000;
    assert(sample(100, now));
    assert(frame.end_tape.id == 1 && frame.end_tape.first_us == 1160000);
    if (i < 10)
      assert(!gate.observe(loc::MazeMap::q1(), frame, p, now));
    else {
      p.timestamp_us = now;
      p.tracking = true;
      assert(gate.observe(loc::MazeMap::q1(), frame, p, now));
    }
  }
  // An interruption cannot refresh an old event after reacquisition on black.
  raw.saturated = true;
  assert(!sample(100, 1680000));
  raw.saturated = false;
  assert(!sample(100, 1720000));
  assert(!sample(100, 1760000));
  // Existing gate tests separately require a fresh pose, the mapped entrance
  // geometry, and reject expired or previously consumed cue identities.
}

void headings() {
  auto c = nav::default_resolver_config();
  unsigned counts[4]{};
  for (unsigned v = 0; v < 4; ++v)
    for (float degrees : {17.f, 37.f, 123.f, -41.f, -137.f})
      for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x) {
          nav::FeatureConfig cfg; // All four variants compete, no color input.
          auto fixture = c;
          fixture.start_count = 1;
          fixture.starts[0].cell = loc::rotate_cell({x, y}, (4 - v) % 4);
          nav::FeatureNavigation e(fixture, cfg);
          nav::FeatureSnapshot s;
          const float yaw = loc::deg2rad(degrees), px = (x + .5f) * .2f,
                      py = (y + .5f) * .2f;
          for (int k = 0; k < 3; ++k) {
            int64_t t = 1000000 + k * 50000;
            nav::BootstrapFrame f;
            f.imu_us = f.mag_us = t;
            f.magnetic_clean = true;
            f.mag = {45 * std::cos(yaw), -45 * std::sin(yaw), 0};
            for (int j = 0; j < 3; ++j) {
              f.tof_us[j] = t;
              f.range_m[j] = loc::trace_maze_ray(c.variants[v], px, py, yaw,
                                                 loc::cfg::lidar_mounts[j])
                                 .range;
            }
            s = e.update(f, t);
          }
          if (s.pose.tracking) {
            assert(unsigned(s.resolution.kind) == v);
            assert(std::hypot(s.pose.x_m - px, s.pose.y_m - py) < 1e-4f);
            assert(std::fabs(loc::wrap_pi(s.pose.yaw_rad - yaw)) < 1e-5f);
            ++counts[v];
          }
        }
  for (auto count : counts)
    assert(count > 0);
  std::printf("Arbitrary heading, no colors: unique Q1..Q4 matches "
              "%u/%u/%u/%u; other poses held\n",
              counts[0], counts[1], counts[2], counts[3]);
}
} // namespace
int main() {
  detector();
  gate();
  pose_only_goal();
  continuous_tape_pose_recovery();
  headings();
  std::puts("PASS end tape debounce, mapped entrance, center-only completion, "
            "arbitrary startup bearing");
}
