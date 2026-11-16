// SPDX-License-Identifier: GPL-3.0-only

#include "localization/maze_map.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace loc {
bool valid_cell(Cell c) {
  return c.x >= 0 && c.x < kMazeN && c.y >= 0 && c.y < kMazeN;
}
Cell rotate_cell(Cell c, unsigned turns) {
  if (!valid_cell(c))
    return c;
  for (unsigned k = 0; k < turns % 4; ++k)
    c = {kMazeN - 1 - c.y, c.x};
  return c;
}
MazeMap MazeMap::q1() {
  MazeMap m;
  m.kind = MapKind::Q1;
  for (int r = 0; r < kMazeN; ++r)
    for (int c = 0; c < kMazeN; ++c) {
      m.walls[r][c] = maze_q1_orig[r][c];
      m.known[r][c] = 15;
    }
  return m;
}
MazeMap MazeMap::rotated(unsigned turns, MapKind label) const {
  MazeMap out;
  out.kind = label;
  for (int r = 0; r < kMazeN; ++r)
    for (int c = 0; c < kMazeN; ++c) {
      Cell to = rotate_cell({c, kMazeN - 1 - r}, turns);
      uint8_t w = walls[r][c], k = known[r][c];
      for (unsigned i = 0; i < turns % 4; ++i) {
        w = uint8_t((w >> 1) | ((w << 3) & 15));
        k = uint8_t((k >> 1) | ((k << 3) & 15));
      }
      out.walls[kMazeN - 1 - to.y][to.x] = w;
      out.known[kMazeN - 1 - to.y][to.x] = k;
    }
  return out;
}
bool MazeMap::valid() const {
  if (static_cast<unsigned>(kind) > 3)
    return false;
  for (int r = 0; r < kMazeN; ++r)
    for (int c = 0; c < kMazeN; ++c)
      if ((walls[r][c] | known[r][c]) & 0xF0)
        return false;
  return true;
}
EdgeState MazeMap::edge(Cell c, uint8_t bit) const {
  if (!valid_cell(c) ||
      (bit != NORTH && bit != EAST && bit != SOUTH && bit != WEST))
    return EdgeState::UNKNOWN;
  Cell n = c;
  uint8_t opposite = 0;
  if (bit == NORTH) {
    ++n.y;
    opposite = SOUTH;
  }
  if (bit == SOUTH) {
    --n.y;
    opposite = NORTH;
  }
  if (bit == EAST) {
    ++n.x;
    opposite = WEST;
  }
  if (bit == WEST) {
    --n.x;
    opposite = EAST;
  }
  const int row = kMazeN - 1 - c.y;
  const bool ka = (known[row][c.x] & bit) != 0,
             wa = (walls[row][c.x] & bit) != 0;
  if (ka && wa)
    return EdgeState::CLOSED;
  if (!valid_cell(n))
    return ka ? EdgeState::OPEN : EdgeState::UNKNOWN;
  const int nr = kMazeN - 1 - n.y;
  const bool kb = (known[nr][n.x] & opposite) != 0,
             wb = (walls[nr][n.x] & opposite) != 0;
  if (kb && wb)
    return EdgeState::CLOSED;
  return ka && kb ? EdgeState::OPEN : EdgeState::UNKNOWN;
}
unsigned MazeMap::contradictions() const {
  unsigned count = 0;
  for (int r = 0; r < kMazeN; ++r)
    for (int c = 0; c < kMazeN; ++c) {
      if (c + 1 < kMazeN && (known[r][c] & EAST) && (known[r][c + 1] & WEST) &&
          bool(walls[r][c] & EAST) != bool(walls[r][c + 1] & WEST))
        ++count;
      if (r + 1 < kMazeN && (known[r][c] & SOUTH) &&
          (known[r + 1][c] & NORTH) &&
          bool(walls[r][c] & SOUTH) != bool(walls[r + 1][c] & NORTH))
        ++count;
    }
  return count;
}
bool MazeMap::can_step(Cell a, Cell b) const {
  if (!valid_cell(a) || !valid_cell(b))
    return false;
  const int dx = b.x - a.x, dy = b.y - a.y;
  uint8_t bit = 0;
  if (dx == 1 && dy == 0)
    bit = EAST;
  if (dx == -1 && dy == 0)
    bit = WEST;
  if (dx == 0 && dy == 1)
    bit = NORTH;
  if (dx == 0 && dy == -1)
    bit = SOUTH;
  return bit && edge(a, bit) == EdgeState::OPEN;
}
MazeRayHit trace_maze_ray(const MazeMap &m, float x, float y, float yaw,
                          const LidarMount &mount, float ambiguity) {
  MazeRayHit out;
  out.range = std::numeric_limits<float>::quiet_NaN();
  if (!m.valid() || !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(yaw) || !std::isfinite(mount.x_body_m) ||
      !std::isfinite(mount.y_body_m) || !std::isfinite(mount.yaw_body_rad) ||
      !std::isfinite(ambiguity) || ambiguity < 0)
    return out;
  const float c = std::cos(yaw), s = std::sin(yaw);
  const float ox = x + c * mount.x_body_m - s * mount.y_body_m;
  const float oy = y + s * mount.x_body_m + c * mount.y_body_m;
  const float extent = kMazeN * kCellM;
  if (ox <= 0 || ox >= extent || oy <= 0 || oy >= extent)
    return out;
  const float dx = std::cos(yaw + mount.yaw_body_rad),
              dy = std::sin(yaw + mount.yaw_body_rad);
  const float inf = std::numeric_limits<float>::infinity();
  constexpr float eps = 1e-6f;
  const float exit_x =
      std::fabs(dx) < eps ? inf : ((dx > 0 ? extent : 0) - ox) / dx;
  const float exit_y =
      std::fabs(dy) < eps ? inf : ((dy > 0 ? extent : 0) - oy) / dy;
  const float exit = std::min(exit_x, exit_y);
  float nearest = inf, second = inf, unknown = inf;
  // Unique grid edges; merge collinear adjacent spans with the same edge state.
  for (int axis = 0; axis < 2; ++axis)
    for (int line = 0; line <= kMazeN; ++line) {
      int start = 0;
      while (start < kMazeN) {
        auto state_at = [&](int span) {
          if (axis == 0)
            return m.edge({line == 0 ? 0 : line - 1, span},
                          line == 0 ? WEST : EAST);
          return m.edge({span, line == 0 ? 0 : line - 1},
                        line == 0 ? SOUTH : NORTH);
        };
        const EdgeState state = state_at(start);
        int end = start + 1;
        while (end < kMazeN && state_at(end) == state)
          ++end;
        if (state == EdgeState::OPEN) {
          start = end;
          continue;
        }
        const float coordinate = line * kCellM, lo = start * kCellM,
                    hi = end * kCellM;
        const float normal_o = axis == 0 ? ox : oy,
                    normal_d = axis == 0 ? dx : dy;
        const float along_o = axis == 0 ? oy : ox,
                    along_d = axis == 0 ? dy : dx;
        if (std::fabs(normal_o - coordinate) <= eps && along_o >= lo - eps &&
            along_o <= hi + eps)
          return out;
        float t = inf, along = 0;
        bool tangent = false;
        if (std::fabs(normal_d) >= eps) {
          t = (coordinate - normal_o) / normal_d;
          along = along_o + t * along_d;
          if (along < lo - eps || along > hi + eps) {
            start = end;
            continue;
          }
        } else if (std::fabs(normal_o - coordinate) <= eps &&
                   std::fabs(along_d) >= eps) {
          along = along_d > 0 ? lo : hi;
          t = (along - along_o) / along_d;
          tangent = true;
        }
        if (t <= eps || t > exit + eps || !std::isfinite(t)) {
          start = end;
          continue;
        }
        if (state == EdgeState::UNKNOWN)
          unknown = std::min(unknown, t);
        else if (t < nearest) {
          second = nearest;
          nearest = t;
          out.axis = axis;
          out.line_m = coordinate;
          out.wall_id = axis * 30 + line * kMazeN + start;
          out.endpoint_distance_m =
              std::max(0.0f, std::min(along - lo, hi - along));
          out.ambiguous = tangent || out.endpoint_distance_m < ambiguity;
        } else
          second = std::min(second, t);
        start = end;
      }
    }
  if (unknown <= nearest + eps && std::isfinite(unknown)) {
    out.status = RayStatus::UNKNOWN_EDGE;
    return out;
  }
  if (!std::isfinite(nearest)) {
    out.status = RayStatus::NO_HIT;
    return out;
  }
  out.status = RayStatus::HIT;
  out.range = nearest;
  out.ambiguous = out.ambiguous || second - nearest < ambiguity;
  return out;
}
} // namespace loc
