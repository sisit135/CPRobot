// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization/config.hpp"
#include "localization/maze_map.hpp"
#include "motor_control/config.hpp"
#include <algorithm>
#include <cmath>

namespace nav {
// Minimum center-to-segment distance. Unknown edges and the arena boundary
// are closed. A pivot is safe only if this exceeds the swept chassis radius
// plus the caller's clearance margin.
float mapped_pivot_clearance(const loc::MazeMap &map, float x_m, float y_m);
// Project the measured hit, including its sensor origin, onto the cardinal
// wall normal associated with the beam. Euclidean mount radius would overstate
// clearance for a side sensor shifted forward along the chassis.
inline float cardinal_wall_clearance(float range_m,
                                     const loc::LidarMount &mount,
                                     float heading_error_rad) {
  const float nx = std::cos(mount.yaw_body_rad);
  const float ny = std::sin(mount.yaw_body_rad);
  return (range_m + nx * mount.x_body_m + ny * mount.y_body_m) *
             std::cos(heading_error_rad) +
         (ny * mount.x_body_m - nx * mount.y_body_m) *
             std::sin(heading_error_rad);
}
// Segment/polygon intersection in the chassis frame. Unknown edges and outer
// boundaries are closed. Pad the triangular head and rear envelope by 1 mm.
inline bool mapped_footprint_clear(const loc::MazeMap &map, float x, float y,
                                   float yaw, float margin = .001f) {
  const float c = std::cos(yaw), s = std::sin(yaw);
  auto touches = [&](float ax, float ay, float bx, float by) {
    const robot::geometry::BodyPoint a = {c * (ax - x) + s * (ay - y),
                                          -s * (ax - x) + c * (ay - y)};
    const robot::geometry::BodyPoint b = {c * (bx - x) + s * (by - y),
                                          -s * (bx - x) + c * (by - y)};
    return robot::geometry::footprint_intersects_segment(a, b, margin);
  };
  if (x <= 0 || y <= 0 || x >= 1 || y >= 1)
    return false;
  for (int gy = 0; gy < loc::kMazeN; ++gy)
    for (int gx = 0; gx < loc::kMazeN; ++gx) {
      const float a = gx * loc::kCellM, b = gy * loc::kCellM;
      if ((gx == 0 && touches(0, b, 0, b + loc::kCellM)) ||
          (gy == 0 && touches(a, 0, a + loc::kCellM, 0)) ||
          ((gx == loc::kMazeN - 1 ||
            map.edge({gx, gy}, loc::EAST) != loc::EdgeState::OPEN) &&
           touches(a + loc::kCellM, b, a + loc::kCellM, b + loc::kCellM)) ||
          ((gy == loc::kMazeN - 1 ||
            map.edge({gx, gy}, loc::NORTH) != loc::EdgeState::OPEN) &&
           touches(a, b + loc::kCellM, a + loc::kCellM, b + loc::kCellM)))
        return false;
    }
  return true;
}

inline bool mapped_axle_pivot_clear(const loc::MazeMap &map, float x, float y,
                                    float yaw, float angle,
                                    float margin = .002f) {
  const int steps = 1 + int(std::fabs(angle) / loc::deg2rad(1));
  const float axle = motor::cfg::wheel_mount_x_body_m;
  for (int i = 0; i <= steps; ++i) {
    const float a = yaw + angle * float(i) / steps;
    if (!mapped_footprint_clear(map, x + axle * (std::cos(yaw) - std::cos(a)),
                                y + axle * (std::sin(yaw) - std::sin(a)), a,
                                margin))
      return false;
  }
  return true;
}

// A v2 posture must keep its forward-shifted side sensors outside their blind
// zone during the proven axle arc. Add 2 mm to the 15 mm sensor limit for
// noise; the physical footprint and the live measurement gates remain
// unchanged.
inline bool mapped_observable_axle_pivot_clear(const loc::MazeMap &map, float x,
                                               float y, float yaw, float angle,
                                               float margin = .002f) {
  if (!mapped_axle_pivot_clear(map, x, y, yaw, angle, margin))
    return false;
  const int steps = 1 + int(std::fabs(angle) / loc::deg2rad(1));
  const float axle = motor::cfg::wheel_mount_x_body_m;
  for (int i = 0; i <= steps; ++i) {
    const float a = yaw + angle * float(i) / steps;
    const float px = x + axle * (std::cos(yaw) - std::cos(a));
    const float py = y + axle * (std::sin(yaw) - std::sin(a));
    for (const auto &mount : loc::cfg::lidar_mounts) {
      const auto hit = loc::trace_maze_ray(map, px, py, a, mount);
      if (hit.status != loc::RayStatus::HIT ||
          hit.range <= loc::cfg::tof_min_m + .002f ||
          hit.range >= loc::cfg::tof_max_m)
        return false;
    }
  }
  return true;
}

} // namespace nav
