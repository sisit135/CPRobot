// SPDX-License-Identifier: GPL-3.0-only
#include "navigation/types.hpp"
#include "start_validation.hpp"

#include <cassert>
#include <cstdio>

int main() {
  using lifecycle::StartValidationReason;
  using loc::Cell;
  const auto q1 = loc::MazeMap::q1();
  const auto config = nav::default_resolver_config();
  const Cell q1_starts[] = {{0, 4}, {4, 4}, {4, 0}};
  assert(config.start_count == 3);
  for (unsigned i = 0; i < config.start_count; ++i)
    assert(config.starts[i].cell == q1_starts[i]);
  const Cell q1_goal{0, 0};

  for (unsigned variant = 0; variant < 4; ++variant) {
    const auto map = q1.rotated(variant, static_cast<loc::MapKind>(variant));
    const auto goal = loc::rotate_cell(q1_goal, variant);
    for (const auto q1_start : q1_starts) {
      const auto start = loc::rotate_cell(q1_start, variant);
      const auto result = lifecycle::validate_start(map, start, goal);
      assert(result.reason == StartValidationReason::REACHABLE);
      assert(result.path.size() >= 2);
      assert(result.path[0] == start);
      assert(result.path[result.path.size() - 1] == goal);
      for (std::size_t i = 1; i < result.path.size(); ++i)
        assert(map.can_step(result.path[i - 1], result.path[i]));
    }
  }

  auto closed_bridge = q1;
  closed_bridge.walls[4][1] |= loc::NORTH;
  const auto rejected =
      lifecycle::validate_start(closed_bridge, {4, 0}, q1_goal);
  assert(rejected.reason == StartValidationReason::NO_PATH);
  assert(rejected.path.empty());
  std::printf("START_VALIDATION REJECTED reason=%s map=Q1 start=(4,0) "
              "goal=(0,0) route_cells=%zu\n",
              lifecycle::start_validation_reason(rejected.reason),
              rejected.path.size());

  loc::MazeMap invalid_map;
  assert(lifecycle::validate_start(invalid_map, {4, 0}, q1_goal).reason ==
         StartValidationReason::INVALID_MAP);
  assert(lifecycle::validate_start(q1, {-1, 0}, q1_goal).reason ==
         StartValidationReason::INVALID_START);
  assert(lifecycle::validate_start(q1, {4, 0}, {5, 3}).reason ==
         StartValidationReason::INVALID_GOAL);
  assert(lifecycle::validate_start(q1, q1_goal, q1_goal).reason ==
         StartValidationReason::SAME_CELL);

  std::puts("PASS startup traversability: all three ordered POIs across Q1-Q4; "
            "closed bridge and invalid inputs rejected");
}
