// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "AStar.hpp"

namespace lifecycle {
enum class StartValidationReason {
  REACHABLE,
  INVALID_MAP,
  INVALID_START,
  INVALID_GOAL,
  SAME_CELL,
  NO_PATH
};

struct StartValidation {
  StartValidationReason reason = StartValidationReason::INVALID_MAP;
  AStar::CoordinateList path{};

  bool reachable() const { return reason == StartValidationReason::REACHABLE; }
};

inline const char *start_validation_reason(StartValidationReason reason) {
  switch (reason) {
  case StartValidationReason::REACHABLE:
    return "REACHABLE";
  case StartValidationReason::INVALID_MAP:
    return "INVALID_MAP";
  case StartValidationReason::INVALID_START:
    return "INVALID_START";
  case StartValidationReason::INVALID_GOAL:
    return "INVALID_GOAL";
  case StartValidationReason::SAME_CELL:
    return "SAME_CELL";
  case StartValidationReason::NO_PATH:
    return "NO_PATH";
  }
  return "UNKNOWN";
}

inline StartValidation validate_start(const loc::MazeMap &map, loc::Cell start,
                                      loc::Cell goal) {
  StartValidation result;
  if (!map.valid()) {
    result.reason = StartValidationReason::INVALID_MAP;
    return result;
  }
  if (!loc::valid_cell(start)) {
    result.reason = StartValidationReason::INVALID_START;
    return result;
  }
  if (!loc::valid_cell(goal)) {
    result.reason = StartValidationReason::INVALID_GOAL;
    return result;
  }
  if (start == goal) {
    result.reason = StartValidationReason::SAME_CELL;
    return result;
  }

  AStar::Generator planner;
  if (!planner.setMaze(map)) {
    result.reason = StartValidationReason::INVALID_MAP;
    return result;
  }
  result.path = planner.findPath(start, goal);
  result.reason = planner.status() == AStar::PathStatus::FOUND
                      ? StartValidationReason::REACHABLE
                      : StartValidationReason::NO_PATH;
  return result;
}
} // namespace lifecycle
