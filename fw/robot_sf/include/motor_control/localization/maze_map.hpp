#pragma once
#include "localization/types.hpp"
#include <cstdint>

namespace loc {
constexpr int kMazeN = 5;
constexpr float kCellM = 0.20f;
enum class MapKind : uint8_t { Q1, Q2, Q3, Q4, UNKNOWN };
enum WallBit : uint8_t { NORTH = 1, EAST = 2, SOUTH = 4, WEST = 8 };
enum class EdgeState : uint8_t { UNKNOWN, OPEN, CLOSED };
struct Cell {
  int x = -1, y = -1; // World cell coordinates, y increases north.
  bool operator==(const Cell &b) const { return x == b.x && y == b.y; }
  bool operator!=(const Cell &b) const { return !(*this == b); }
};
inline constexpr uint8_t maze_q1_orig[kMazeN][kMazeN] = {
    {0b1001, 0b0101, 0b0001, 0b0101, 0b0111},
    {0b1110, 0b1011, 0b1100, 0b0001, 0b0011},
    {0b1000, 0b0010, 0b1001, 0b0110, 0b0010},
    {0b1010, 0b1010, 0b0100, 0b0011, 0b0010},
    {0b1110, 0b1101, 0b0100, 0b0110, 0b1110}};
struct MazeMap {
  uint8_t walls[kMazeN][kMazeN]{}; // row 0 = NORTH, row 4 = SOUTH.
  uint8_t known[kMazeN]
               [kMazeN]{}; // One known bit per direction; zero = unknown.
  MapKind kind = MapKind::UNKNOWN;
  /** @brief Create the supplied Q1 map without silently modifying its masks.
   * @returns Q1 map with all four edge bits marked known in every cell.
   */
  static MazeMap q1();
  /** @brief Rotate a complete or partial map counterclockwise around its
   * center.
   * @param[in] turns Number of quarter turns, reduced modulo four.
   * @param[in] new_kind Label for the returned variant.
   * @returns Rotated masks and knowledge flags in the same world frame.
   */
  MazeMap rotated(unsigned turns, MapKind new_kind) const;
  /** @brief Validate masks and variant label.
   * @returns true if masks use bits 0..3 and kind is a recognized variant.
   */
  bool valid() const;
  /** @brief Count contradictory known shared edges once each.
   * @returns Number of E/W or N/S disagreements; Q1 returns six.
   */
  unsigned contradictions() const;
  /** @brief Resolve one physical cell edge using both adjacent masks.
   * @param[in] cell World cell index in 0..4.
   * @param[in] bit Exactly NORTH, EAST, SOUTH or WEST.
   * @returns CLOSED if either known side says closed; OPEN only if all
   * relevant sides say known-open; UNKNOWN otherwise or for invalid indices.
   */
  EdgeState edge(Cell cell, uint8_t bit) const;
  /** @brief Determine whether a cardinal cell-to-cell move is traversable.
   * @param[in] from Valid source cell.
   * @param[in] to Valid destination cell, exactly one cardinal step away.
   * @returns true only for an in-map, known-open shared edge.
   */
  bool can_step(Cell from, Cell to) const;
};
/** @brief Validate a cell coordinate.
 * @param[in] cell World indices.
 * @returns true for both indices in 0..4.
 */
bool valid_cell(Cell cell);
/** @brief Rotate a world cell index counterclockwise.
 * @param[in] cell Original world cell.
 * @param[in] turns Quarter turns modulo four.
 * @returns Rotated cell; invalid input is returned unchanged.
 */
Cell rotate_cell(Cell cell, unsigned turns);

enum class RayStatus : uint8_t { HIT, NO_HIT, UNKNOWN_EDGE, INVALID };
struct MazeRayHit {
  float range = 0, line_m = 0, endpoint_distance_m = 0;
  int axis = -1, wall_id = -1; // axis 0=X-normal, 1=Y-normal; IDs 0..59.
  bool ambiguous = false;
  RayStatus status = RayStatus::INVALID;
};
/** @brief Trace a sensor beam against finite mapped wall segments.
 * @param[in] map Complete or partial world-frame maze snapshot.
 * @param[in] x Robot-center world X in meters.
 * @param[in] y Robot-center world Y in meters.
 * @param[in] yaw Robot CCW heading in radians.
 * @param[in] mount Beam origin and heading relative to the robot.
 * @param[in] ambiguity_m Endpoint/competing-wall exclusion distance.
 * @returns First known wall hit; NaN range on unknown-before-hit, no hit,
 * or invalid/on-wall origin. Collinear wall spans are merged before tracing.
 */
MazeRayHit trace_maze_ray(const MazeMap &map, float x, float y, float yaw,
                          const LidarMount &mount, float ambiguity_m = 0.02f);
} // namespace loc
