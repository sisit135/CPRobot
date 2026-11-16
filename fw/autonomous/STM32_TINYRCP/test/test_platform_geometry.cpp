// SPDX-License-Identifier: GPL-3.0-only
#include "navigation/turn_clearance.hpp"
#include <cassert>
#include <cmath>

namespace {
void near(float a, float b) { assert(std::fabs(a - b) < 1e-6f); }
} // namespace
int main() {
  using namespace robot::geometry;
  near(wheel_mounts[0].x_m, -.054175144f);
  near(wheel_mounts[1].x_m, .054175144f);
  near(wheel_mounts[0].y_m, -.024f);
  near(wheel_mounts[1].y_m, -.024f);
  near(tof_mounts[0].x_m, -.067175144f);
  near(tof_mounts[2].x_m, .067175144f);
  near(tof_mounts[0].y_m, .0225f);
  near(tof_mounts[2].y_m, .0225f);
  if (platform_v2) {
    near(platform_width_m, .135f);
    near(platform_length_m, .110f);
    near(loc::cfg::lidar_mounts[1].x_body_m, .0316f);
    near(loc::cfg::lidar_mounts[1].y_body_m, -.0022f);
    // A 48.4 mm raw return is only 25 mm clear of the front platform edge.
    near(front_edge_range(.0484f), .025f);
    auto map = loc::MazeMap::q1();
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < 5; ++col)
        map.walls[row][col] =
            (row == 0 ? loc::NORTH : 0) | (row == 4 ? loc::SOUTH : 0) |
            (col == 0 ? loc::WEST : 0) | (col == 4 ? loc::EAST : 0);
    // Shortened front clears a wall 55.5 mm away, including when reversed.
    assert(nav::mapped_footprint_clear(map, .9445f, .5f, 0, 0));
    assert(nav::mapped_footprint_clear(map, .0555f, .5f, loc::kPi, 0));
    // A platform-only check would miss the rear tires extending to -56.5 mm.
    assert(!nav::mapped_footprint_clear(map, .056f, .5f, 0, 0));
    assert(nav::mapped_footprint_clear(map, .0566f, .5f, 0, 0));
    assert(!nav::mapped_footprint_clear(map, .5f, .067f, 0, 0));
    auto corridor = map;
    for (auto &row : corridor.walls)
      for (auto &walls : row)
        walls |= loc::EAST | loc::WEST;
    // The chassis arc fits, but the forward side sensor sees less than the
    // 17 mm planning reserve. A 4 mm bias restores optical visibility.
    assert(nav::mapped_axle_pivot_clear(corridor, .483f, .5f, loc::kPi / 2,
                                        -loc::kPi / 2));
    assert(!nav::mapped_observable_axle_pivot_clear(
        corridor, .483f, .5f, loc::kPi / 2, -loc::kPi / 2));
    assert(nav::mapped_observable_axle_pivot_clear(
        corridor, .487f, .5f, loc::kPi / 2, -loc::kPi / 2));

  } else {
    near(platform_diagonal_m, .190f);
    near(tof_mounts[1].x_m, 0);
    near(tof_mounts[1].y_m, .067175144f);
    near(front_edge_range(.025f), .025f);
  }
}
