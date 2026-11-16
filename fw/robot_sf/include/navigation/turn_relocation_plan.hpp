// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "navigation/path_executor.hpp"
#include "navigation/turn_clearance.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nav {
// Planning evidence only. This type does not authorize a motor command.
struct TurnRelocationPlan {
  loc::Cell cell{};
  float x_m = 0, y_m = 0, heading_rad = 0, exit_yaw_rad = 0;
  float travel_m = 0;
  unsigned extra_steps = 0, travel_steps = 0;
  bool reverse = false, valid = false;
};
inline bool relocation_sweep_clear(const loc::MazeMap &map, float x, float y,
                                   float yaw, float tx, float ty,
                                   float target_yaw, float margin = .002f) {
  const float angle = loc::wrap_pi(target_yaw - yaw);
  const int steps = 1 + int(std::max(std::hypot(tx - x, ty - y) / .001f,
                                     std::fabs(angle) / loc::deg2rad(1)));
  for (int i = 0; i <= steps; ++i) {
    const float t = float(i) / steps;
    if (!mapped_footprint_clear(map, x + t * (tx - x), y + t * (ty - y),
                                yaw + t * angle, margin))
      return false;
  }
  return true;
}
// A blocked turn cannot be required on the first leg to its recovery cell.
// Search forward/reverse along the current known-open corridor. Minimize the
// added shortest-path length to the goal, then distance to the correction cell.
inline TurnRelocationPlan plan_turn_relocation(const loc::MazeMap &map,
                                               const loc::PoseSnapshot &p,
                                               loc::Cell goal,
                                               uint32_t excluded = 0,
                                               uint32_t failed_cells = 0) {
  TurnRelocationPlan best{};
  const loc::Cell start{p.grid_x, p.grid_y};
  if (!map.valid() || !loc::valid_cell(start) || !loc::valid_cell(goal) ||
      !p.tracking || !std::isfinite(p.x_m) || !std::isfinite(p.y_m) ||
      !std::isfinite(p.yaw_rad) || p.x_m <= 0 || p.y_m <= 0 || p.x_m >= 1 ||
      p.y_m >= 1 || int(p.x_m / loc::kCellM) != start.x ||
      int(p.y_m / loc::kCellM) != start.y)
    return best;
  const int direction =
      (int(std::lround(loc::wrap_pi(p.yaw_rad) / (loc::kPi / 2))) + 4) % 4;
  const float yaw = direction * loc::kPi / 2;
  if (std::fabs(loc::wrap_pi(p.yaw_rad - yaw)) > loc::deg2rad(5))
    return best;
  constexpr int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
  constexpr uint8_t bits[4] = {loc::EAST, loc::NORTH, loc::WEST, loc::SOUTH};
  AStar::Generator planner;
  planner.setMaze(map);
  const auto direct = planner.findPath(start, goal);
  if (direct.empty())
    return best;
  // A recovery route must not immediately return to the cell whose turn
  // failed. The goal itself remains reachable as a final arrival.
  const uint32_t avoid =
      (failed_cells | (uint32_t(1) << (start.y * 5 + start.x))) &
      ~(uint32_t(1) << (goal.y * 5 + goal.x));
  for (int i = 0; i < 25; ++i)
    if (avoid & (uint32_t(1) << i))
      planner.addCollision({i % 5, i / 5});
  for (int sign : {1, -1}) {
    loc::Cell previous = start;
    for (unsigned steps = 1; steps < loc::kMazeN; ++steps) {
      const loc::Cell cell{previous.x + sign * dx[direction],
                           previous.y + sign * dy[direction]};
      if (!map.can_step(previous, cell))
        break;
      previous = cell;
      if (excluded & (uint32_t(1) << (cell.y * loc::kMazeN + cell.x)))
        continue;
      if (map.edge(cell, bits[(direction + 1) % 4]) != loc::EdgeState::OPEN &&
          map.edge(cell, bits[(direction + 3) % 4]) != loc::EdgeState::OPEN)
        continue;
      const auto resume = planner.findPath(cell, goal);
      if (resume.empty())
        continue;
      float exit_yaw = yaw;
      if (resume.count > 1)
        exit_yaw = std::atan2(float(resume[1].y - cell.y),
                              float(resume[1].x - cell.x));
      const unsigned extra = steps + resume.count - direct.count;
      if (best.valid && extra > best.extra_steps)
        continue;
      for (float offset : {0.f, -.012f, .012f, -.024f, .024f}) {
        const float x =
            dx[direction] ? (cell.x + .5f) * loc::kCellM + offset : p.x_m;
        const float y =
            dy[direction] ? (cell.y + .5f) * loc::kCellM + offset : p.y_m;
        if (!relocation_sweep_clear(map, p.x_m, p.y_m, p.yaw_rad, x, y,
                                    p.yaw_rad) ||
            !mapped_axle_pivot_clear(map, x, y, yaw,
                                     loc::wrap_pi(exit_yaw - yaw), .0024f))
          continue;
        const float travel = std::hypot(x - p.x_m, y - p.y_m);
        if (!best.valid || extra < best.extra_steps ||
            (extra == best.extra_steps && travel < best.travel_m - .0001f))
          best = {cell,   x,     y,     yaw,      exit_yaw,
                  travel, extra, steps, sign < 0, true};
        break;
      }
    }
  }
  return best;
}
} // namespace nav
