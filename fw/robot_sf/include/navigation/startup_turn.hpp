// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "localization/config.hpp"
#include "navigation/feature_navigation.hpp"
#include "navigation/turn_clearance.hpp"
#include <algorithm>
#include <cmath>

namespace nav {
namespace startup_turn_detail {
inline bool fresh(int64_t stamp, int64_t now, int64_t max_age) {
  return stamp > 0 && stamp <= now && now - stamp <= max_age;
}
inline uint8_t bit_for(int direction) {
  static constexpr uint8_t bits[4] = {loc::EAST, loc::NORTH, loc::WEST,
                                      loc::SOUTH};
  return bits[(direction + 4) % 4];
}
inline bool cardinal(float yaw, int &direction) {
  if (!std::isfinite(yaw))
    return false;
  const float quarter = loc::kPi / 2;
  direction = (int(std::lround(loc::wrap_pi(yaw) / quarter)) + 4) % 4;
  return std::fabs(loc::wrap_pi(yaw - direction * quarter)) <= loc::deg2rad(5);
}
inline bool measure_center(const loc::MazeMap &map, const BootstrapFrame &frame,
                           float yaw, int direction, loc::Cell cell,
                           float &clearance) {
  const float axis_x[4] = {1, 0, -1, 0};
  const float axis_y[4] = {0, 1, 0, -1};
  float estimates_x[3]{}, estimates_y[3]{};
  unsigned count_x = 0, count_y = 0;
  const float cs = std::cos(yaw), sn = std::sin(yaw);
  // Sensors are ordered left, front, right. Each must measure the expected
  // closed cell edge; an unrelated nearby return cannot locate the center.
  const int ray_direction[3] = {(direction + 1) % 4, direction,
                                (direction + 3) % 4};
  for (unsigned i = 0; i < 3; ++i) {
    const int d = ray_direction[i];
    const auto &mount = loc::cfg::lidar_mounts[i];
    const float ox = cs * mount.x_body_m - sn * mount.y_body_m;
    const float oy = sn * mount.x_body_m + cs * mount.y_body_m;
    const float beam = yaw + mount.yaw_body_rad;
    const float projection =
        axis_x[d] * std::cos(beam) + axis_y[d] * std::sin(beam);
    if (!std::isfinite(frame.range_m[i]) || frame.range_m[i] <= 0 ||
        frame.range_m[i] >= 2 || projection < .98f)
      return false;
    const float radial_clearance =
        axis_x[d] * ox + axis_y[d] * oy + projection * frame.range_m[i];
    if (!std::isfinite(radial_clearance) ||
        radial_clearance < robot::geometry::footprint_radius_m + .002f)
      return false;
    const float line = (d == 0   ? cell.x + 1
                        : d == 2 ? cell.x
                        : d == 1 ? cell.y + 1
                                 : cell.y) *
                       loc::kCellM;
    const float coordinate =
        axis_x[d] != 0 ? line - ox - std::cos(beam) * frame.range_m[i]
                       : line - oy - std::sin(beam) * frame.range_m[i];
    if (!std::isfinite(coordinate))
      return false;
    if (axis_x[d] != 0)
      estimates_x[count_x++] = coordinate;
    else
      estimates_y[count_y++] = coordinate;
  }
  if (!count_x || !count_y)
    return false;
  float x = 0, y = 0;
  for (unsigned i = 0; i < count_x; ++i)
    x += estimates_x[i];
  for (unsigned i = 0; i < count_y; ++i)
    y += estimates_y[i];
  x /= count_x;
  y /= count_y;
  for (unsigned i = 0; i < count_x; ++i)
    if (std::fabs(estimates_x[i] - x) > .015f)
      return false;
  for (unsigned i = 0; i < count_y; ++i)
    if (std::fabs(estimates_y[i] - y) > .015f)
      return false;
  if (x < cell.x * loc::kCellM || x >= (cell.x + 1) * loc::kCellM ||
      y < cell.y * loc::kCellM || y >= (cell.y + 1) * loc::kCellM)
    return false;
  clearance = INFINITY;
  for (unsigned ix = 0; ix < count_x; ++ix)
    for (unsigned iy = 0; iy < count_y; ++iy) {
      const float candidate_clearance =
          mapped_pivot_clearance(map, estimates_x[ix], estimates_y[iy]);
      if (!std::isfinite(candidate_clearance))
        return false;
      // A platform-center circle misses the translation around the rear axle.
      // RHR's startup U-turn requests positive pi; prove that physical arc
      // independently for every range-derived candidate position.
      if (!mapped_axle_pivot_clear(map, estimates_x[ix], estimates_y[iy], yaw,
                                   loc::kPi))
        return false;
      clearance = std::min(clearance, candidate_clearance);
    }
  return true;
}
} // namespace startup_turn_detail

/** Permit a startup-only in-place U-turn when every surviving resolver
 * hypothesis describes the same safe dead-end signature. No pose or motion
 * truth is consumed; the measured ranges only locate a candidate pivot for a
 * conservative all-wall clearance check.
 */
inline bool startup_wall_turn_allowed(const ResolverConfig &config,
                                      const FeatureSnapshot &feature,
                                      const BootstrapFrame &frame,
                                      int64_t now) {
  using namespace startup_turn_detail;
  if (feature.resolution.status != ResolveStatus::AMBIGUOUS ||
      feature.candidates < 2 || feature.candidates > 32 || now < 0 ||
      !fresh(frame.imu_us, now, 100000) || !std::isfinite(frame.imu.gz_rps) ||
      std::fabs(frame.imu.gz_rps) > .05f ||
      !fresh(feature.heading.timestamp_us, now, 100000) ||
      !feature.heading.valid ||
      !fresh(feature.candidate_epoch_us, now, 150000) ||
      config.start_count == 0 || config.start_count > 8)
    return false;
  int direction = 0;
  if (!cardinal(feature.heading.yaw_rad, direction))
    return false;
  for (unsigned i = 0; i < 3; ++i)
    if (!fresh(frame.tof_us[i], now, 150000))
      return false;

  unsigned total = 0;
  for (unsigned v = 0; v < 4; ++v) {
    const uint32_t mask = feature.candidate_cells[v];
    if (!mask)
      continue;
    if (mask & ~((uint32_t(1) << 25) - 1))
      return false;
    if (!config.variants[v].valid() ||
        config.variants[v].kind != static_cast<loc::MapKind>(v))
      return false;
    for (unsigned index = 0; index < 25; ++index) {
      if (!(mask & (uint32_t(1) << index)))
        continue;
      ++total;
      const loc::Cell cell{int(index % 5), int(index / 5)};
      bool configured_poi = false;
      for (unsigned p = 0; p < config.start_count && p < 8; ++p)
        configured_poi =
            configured_poi ||
            loc::rotate_cell(config.starts[p].cell, config.turns[v]) == cell;
      if (!configured_poi)
        return false;
      const int front = direction;
      const int left = (direction + 1) % 4;
      const int right = (direction + 3) % 4;
      const int rear = (direction + 2) % 4;
      const auto &map = config.variants[v];
      if (map.edge(cell, bit_for(front)) != loc::EdgeState::CLOSED ||
          map.edge(cell, bit_for(left)) != loc::EdgeState::CLOSED ||
          map.edge(cell, bit_for(right)) != loc::EdgeState::CLOSED ||
          map.edge(cell, bit_for(rear)) != loc::EdgeState::OPEN)
        return false;
      float clearance = 0;
      if (!measure_center(map, frame, feature.heading.yaw_rad, direction, cell,
                          clearance))
        return false;
      if (!std::isfinite(clearance) ||
          clearance < robot::geometry::footprint_radius_m + .002f)
        return false;
    }
  }
  return total == feature.candidates;
}
} // namespace nav
