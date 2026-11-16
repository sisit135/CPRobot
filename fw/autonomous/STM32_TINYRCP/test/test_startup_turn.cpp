// SPDX-License-Identifier: GPL-3.0-only

#include "navigation/map_resolver.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/right_hand_rule.hpp"
#include "navigation/startup_turn.hpp"
#include <cassert>
#include <cmath>

namespace {
void set_edge(loc::MazeMap &map, loc::Cell c, uint8_t bit, bool closed) {
  const int row = loc::kMazeN - 1 - c.y;
  map.walls[row][c.x] =
      closed ? map.walls[row][c.x] | bit : map.walls[row][c.x] & ~bit;
  map.known[row][c.x] |= bit;
  loc::Cell other = c;
  uint8_t opposite = 0;
  if (bit == loc::NORTH) {
    ++other.y;
    opposite = loc::SOUTH;
  } else if (bit == loc::EAST) {
    ++other.x;
    opposite = loc::WEST;
  } else if (bit == loc::SOUTH) {
    --other.y;
    opposite = loc::NORTH;
  } else {
    --other.x;
    opposite = loc::EAST;
  }
  if (loc::valid_cell(other)) {
    const int other_row = loc::kMazeN - 1 - other.y;
    map.walls[other_row][other.x] =
        closed ? map.walls[other_row][other.x] | opposite
               : map.walls[other_row][other.x] & ~opposite;
    map.known[other_row][other.x] |= opposite;
  }
}

nav::ResolverConfig safe_config(unsigned turns) {
  nav::ResolverConfig cfg{};
  cfg.start_count = 2;
  cfg.starts[0].cell = {2, 2};
  cfg.starts[1].cell = {2, 0};
  cfg.turns[0] = turns;
  auto &map = cfg.variants[0];
  map.kind = loc::MapKind::Q1;
  for (int row = 0; row < loc::kMazeN; ++row) {
    const int y = loc::kMazeN - 1 - row;
    for (int x = 0; x < loc::kMazeN; ++x) {
      map.known[row][x] = 15;
      if (y == 0)
        map.walls[row][x] |= loc::SOUTH;
      if (y == loc::kMazeN - 1)
        map.walls[row][x] |= loc::NORTH;
      if (x == 0)
        map.walls[row][x] |= loc::WEST;
      if (x == loc::kMazeN - 1)
        map.walls[row][x] |= loc::EAST;
    }
  }
  // For a south-facing robot, E/S/W are closed and N is open.
  set_edge(map, {2, 2}, loc::EAST, true);
  set_edge(map, {2, 2}, loc::SOUTH, true);
  set_edge(map, {2, 2}, loc::WEST, true);
  set_edge(map, {2, 2}, loc::NORTH, false);
  set_edge(map, {2, 0}, loc::EAST, true);
  set_edge(map, {2, 0}, loc::SOUTH, true);
  set_edge(map, {2, 0}, loc::WEST, true);
  set_edge(map, {2, 0}, loc::NORTH, false);
  return cfg;
}

nav::BootstrapFrame frame_at(loc::Cell c, unsigned direction, int64_t stamp) {
  nav::BootstrapFrame f{};
  const float yaw = direction * loc::kPi / 2;
  f.imu_us = stamp;
  f.imu.gz_rps = 0;
  for (unsigned i = 0; i < 3; ++i) {
    f.tof_us[i] = stamp;
    const auto &mount = loc::cfg::lidar_mounts[i];
    const float ox =
        std::cos(yaw) * mount.x_body_m - std::sin(yaw) * mount.y_body_m;
    const float oy =
        std::sin(yaw) * mount.x_body_m + std::cos(yaw) * mount.y_body_m;
    const unsigned d = i == 0   ? (direction + 1) % 4
                       : i == 1 ? direction
                                : (direction + 3) % 4;
    const float dx[4] = {1, 0, -1, 0};
    const float line = (d == 0   ? c.x + 1
                        : d == 2 ? c.x
                        : d == 1 ? c.y + 1
                                 : c.y) *
                       loc::kCellM;
    const float beam = yaw + mount.yaw_body_rad;
    const float projection = dx[d] != 0 ? std::cos(beam) : std::sin(beam);
    const float axis_origin = dx[d] != 0 ? ox : oy;
    const float center_axis =
        dx[d] != 0 ? (c.x + .5f) * loc::kCellM : (c.y + .5f) * loc::kCellM;
    f.range_m[i] = (line - center_axis - axis_origin) / projection;
  }
  return f;
}

nav::FeatureSnapshot feature_for(loc::Cell c, unsigned variant, int64_t stamp,
                                 float yaw, unsigned turns) {
  nav::FeatureSnapshot s{};
  s.resolution.status = nav::ResolveStatus::AMBIGUOUS;
  s.candidates = 2;
  s.candidate_epoch_us = stamp;
  const auto second = loc::rotate_cell({2, 0}, turns);
  const unsigned bit0 = unsigned(c.y * 5 + c.x);
  const unsigned bit1 = unsigned(second.y * 5 + second.x);
  s.candidate_cells[variant] = (uint32_t(1) << bit0) | (uint32_t(1) << bit1);
  s.heading = {yaw, stamp, true};
  return s;
}

void rejects_rear_axle_uturn_at_q1_entrances_for_all_rotations() {
  constexpr int64_t now = 1000000;
  const auto cfg = nav::default_resolver_config();
  for (unsigned turns = 0; turns < 4; ++turns) {
    const auto first = loc::rotate_cell(cfg.starts[0].cell, turns);
    const auto second = loc::rotate_cell(cfg.starts[1].cell, turns);
    nav::FeatureSnapshot feature{};
    feature.resolution.status = nav::ResolveStatus::AMBIGUOUS;
    feature.candidates = 2;
    feature.candidate_epoch_us = now;
    feature.candidate_cells[turns] = (uint32_t(1) << (first.y * 5 + first.x)) |
                                     (uint32_t(1) << (second.y * 5 + second.x));
    const unsigned direction = (3 + turns) % 4;
    const float yaw = loc::wrap_pi(direction * loc::kPi / 2);
    feature.heading = {yaw, now, true};
    const auto frame = frame_at(first, direction, now);
    assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  }
}

void rejects_rear_axle_uturn_in_narrow_dead_ends_for_each_rotation() {
  constexpr int64_t now = 1000000;
  for (unsigned turns = 0; turns < 4; ++turns) {
    auto cfg = safe_config(turns);
    if (turns) {
      cfg.variants[turns] =
          cfg.variants[0].rotated(turns, static_cast<loc::MapKind>(turns));
      cfg.turns[turns] = turns;
      cfg.variants[0].kind = loc::MapKind::Q1;
      const auto first = loc::rotate_cell({2, 2}, turns);
      auto feature = feature_for(
          first, turns, now, loc::wrap_pi((3 + turns) * loc::kPi / 2), turns);
      const auto frame = frame_at(first, (3 + turns) % 4, now);
      const bool allowed =
          nav::startup_wall_turn_allowed(cfg, feature, frame, now);
      assert(!allowed);
    } else {
      auto feature = feature_for({2, 2}, 0, now, 3 * loc::kPi / 2, 0);
      const auto frame = frame_at({2, 2}, 3, now);
      const bool allowed =
          nav::startup_wall_turn_allowed(cfg, feature, frame, now);
      assert(!allowed);
    }
  }
}

void rejects_bad_evidence_and_non_poi_candidates() {
  constexpr int64_t now = 1000000;
  auto cfg = safe_config(0);
  auto feature = feature_for({2, 2}, 0, now, 3 * loc::kPi / 2, 0);
  auto frame = frame_at({2, 2}, 3, now);
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  frame.tof_us[1] = now - 150001;
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  frame = frame_at({2, 2}, 3, now);
  feature.candidate_epoch_us = now - 150001;
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  feature.candidate_epoch_us = now;
  feature.heading.yaw_rad += loc::deg2rad(6);
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  feature.heading.yaw_rad = 3 * loc::kPi / 2;
  feature.candidate_cells[0] |= uint32_t(1) << 31;
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  feature.candidate_cells[0] &= ~(uint32_t(1) << 31);
  feature.candidate_cells[0] |= uint32_t(1) << (1 * 5 + 1);
  feature.candidates = 3;
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  feature.candidate_cells = {};
  feature.candidates = 0;
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
}

void rejects_inconsistent_edges_and_ambiguous_counts() {
  constexpr int64_t now = 1000000;
  auto cfg = safe_config(0);
  auto feature = feature_for({2, 2}, 0, now, 3 * loc::kPi / 2, 0);
  auto frame = frame_at({2, 2}, 3, now);
  set_edge(cfg.variants[0], {2, 2}, loc::NORTH, true);
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  set_edge(cfg.variants[0], {2, 2}, loc::NORTH, false);
  feature.candidates = 4;
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  feature.candidates = 2;
  frame.range_m[0] = .020f;
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
  frame.range_m[0] = 2.1f;
  assert(!nav::startup_wall_turn_allowed(cfg, feature, frame, now));
}

void uturn_entry_skips_wall_decision_and_enters_center_guard() {
  nav::RightHandRule rhr{};
  rhr.begin_uturn();
  assert(rhr.phase() == nav::FallbackPhase::CENTER);

  nav::BootstrapFrame frame{};
  motor::MotorSnapshot motor{};
  nav::HeadingObservation heading{};
  int64_t now = 1000000;
  frame.range_m[0] = frame.range_m[2] = .100f - loc::cfg::platform_half_side_m;
  frame.range_m[1] = .115f - loc::cfg::lidar_mounts[1].x_body_m;
  heading.valid = true;
  heading.yaw_rad = 0;
  for (int i = 0; i < 60 && rhr.phase() != nav::FallbackPhase::TURN; ++i) {
    now += 20000;
    frame.imu_us = motor.timestamp_us = heading.timestamp_us = now;
    frame.imu.gz_rps = 0;
    for (auto &stamp : frame.tof_us)
      stamp = now;
    const auto out = rhr.update(frame, motor, now, nullptr, &heading);
    assert((out.linear_m_s > 0) == (nav::motion_test::rolling_turns &&
                                    rhr.phase() == nav::FallbackPhase::TURN));
  }
  assert(rhr.phase() == nav::FallbackPhase::TURN);
}
} // namespace

int main() {
  rejects_rear_axle_uturn_at_q1_entrances_for_all_rotations();
  rejects_rear_axle_uturn_in_narrow_dead_ends_for_each_rotation();
  rejects_bad_evidence_and_non_poi_candidates();
  rejects_inconsistent_edges_and_ambiguous_counts();
  auto open = loc::MazeMap::q1();
  for (auto &row : open.walls)
    for (auto &cell : row)
      cell = 0;
  assert(nav::mapped_axle_pivot_clear(open, .5f, .5f, 0, -loc::kPi));
  const auto narrow = safe_config(0).variants[0];
  assert(nav::mapped_pivot_clearance(narrow, .5f, .5f) > .097f);
  assert(!nav::mapped_axle_pivot_clear(narrow, .5f, .5f, 3 * loc::kPi / 2,
                                       -loc::kPi));
  // The same turn can fit after lateral repositioning; startup must not
  // confuse that posture with the centered, range-derived entrance pose.
  assert(nav::mapped_axle_pivot_clear(narrow, .51675f, .5f, 3 * loc::kPi / 2,
                                      -loc::kPi));
  // The positive-pi RHR request requires the opposite lateral bias. Its
  // proof must not authorize the clockwise-only posture above.
  assert(!nav::mapped_axle_pivot_clear(narrow, .51675f, .5f, 3 * loc::kPi / 2,
                                       loc::kPi));
  assert(nav::mapped_axle_pivot_clear(narrow, .48325f, .5f, 3 * loc::kPi / 2,
                                      loc::kPi));
  uturn_entry_skips_wall_decision_and_enters_center_guard();
}
