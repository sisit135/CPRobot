#pragma once
#include "localization/maze_map.hpp"
#include <cstddef>
namespace AStar {
using Vec2i = loc::Cell;
struct CoordinateList {
  Vec2i cells[loc::kMazeN * loc::kMazeN]{};
  unsigned count = 0;
  std::size_t size() const { return count; }
  bool empty() const { return count == 0; }
  const Vec2i &operator[](std::size_t i) const { return cells[i]; }
  const Vec2i *begin() const { return cells; }
  const Vec2i *end() const { return cells + count; }
};
enum class PathStatus { FOUND, NO_PATH, INVALID_INPUT, NO_MAP };
class Generator {
public:
  /** @brief Copy a physical wall map for subsequent searches.
   * @param[in] map Valid maze with known flags; unknown edges are blocked.
   * @returns true on copy, false for invalid masks/label.
   */
  bool setMaze(const loc::MazeMap &map);
  /** @brief Validate the fixed-capacity world dimensions.
   * @param[in] size Required dimensions {5,5}.
   * @returns true only for the supported 5x5 world.
   */
  bool setWorldSize(Vec2i size) {
    return size.x == loc::kMazeN && size.y == loc::kMazeN;
  }
  /** @brief Select cardinal-only movement for wall-encoded cells.
   * @param[in] enabled False; diagonal moves are unsupported.
   * @returns true for false, false for a diagonal-movement request.
   */
  bool setDiagonalMovement(bool enabled) { return !enabled; }
  /** @brief Mark an occupied cell in addition to physical edge walls.
   * @param[in] cell Cell to mark; invalid indices are ignored.
   * @returns No value.
   */
  void addCollision(Vec2i cell);
  /** @brief Clear one occupied-cell restriction.
   * @param[in] cell Cell to unmark; invalid indices are ignored.
   * @returns No value.
   */
  void removeCollision(Vec2i cell);
  /** @brief Clear occupied-cell restrictions, retaining the maze walls.
   * @returns No value.
   */
  void clearCollisions();
  /** @brief Search known-open cardinal edges with Manhattan A*.
   * @param[in] source Valid unoccupied start cell.
   * @param[in] target Valid unoccupied goal cell.
   * @returns Source-to-target list including both endpoints; empty on failure.
   * @note Fixed 25-node storage; no new/delete, partial-path fallback or
   * diagonals.
   */
  CoordinateList findPath(Vec2i source, Vec2i target);
  /** @brief Inspect the result of the most recent search.
   * @returns FOUND, NO_PATH, INVALID_INPUT or NO_MAP.
   */
  PathStatus status() const { return status_; }

private:
  loc::MazeMap map_{};
  bool blocked_[25]{};
  PathStatus status_ = PathStatus::NO_MAP;
};
} // namespace AStar
