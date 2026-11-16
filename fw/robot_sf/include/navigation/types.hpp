// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "localization/color_tracker.hpp"
#include "localization/maze_map.hpp"
#include <cstdint>
#include <type_traits>
namespace nav {
enum class TileClass : uint8_t {
  UNKNOWN,
  PURPLE_END,
  RED_START,
  BLUE_START,
  OUTLINE,
  NORMAL_Q1Q3,
  NORMAL_Q2Q4
};
struct HeadingObservation {
  float yaw_rad = 0;
  int64_t timestamp_us = 0;
  bool valid = false;
};
struct EndTapeEvent {
  uint32_t id = 0;
  int64_t first_us = 0, confirmed_us = 0;
};

struct BootstrapFrame {
  EndTapeEvent end_tape{};
  loc::RgbcSample color{};
  int64_t color_us = 0, imu_us = 0, mag_us = 0, tof_us[3]{};
  loc::ImuSample imu{};
  loc::MagSample mag{};
  float range_m[3]{};
  bool magnetic_clean = false;
  loc::MarkerSample marker{}; // Latest debounced unique mapped entry, also
                              // available without EKF.
  int64_t marker_us = 0;
};
struct ColorPrototype {
  float r = 0, g = 0, b = 0;
  TileClass tile = TileClass::UNKNOWN;
};
struct StartupPoi {
  loc::Cell cell{};
  TileClass tile = TileClass::UNKNOWN;
  uint8_t heading_mask = 15; // Local E=bit0,N=bit1,W=bit2,S=bit3; 15=ambiguous.
};
struct ResolverConfig {
  loc::MazeMap variants[4]{};
  unsigned turns[4]{0, 1, 2, 3}; // Q1/Q2/Q3/Q4 counterclockwise convention.
  StartupPoi starts[8]{};
  unsigned start_count = 0;
  ColorPrototype colors[6]{};
  unsigned color_count = 0;
  bool colors_calibrated = false;
  float range_sigma_m = 0.04f, max_score = 25, min_margin = 9;
  float heading_tolerance_rad = loc::deg2rad(20);
  unsigned confirmations = 3;
};
enum class ResolveStatus : uint8_t {
  WAIT_CALIBRATION,
  WAIT_SENSORS,
  NO_CANDIDATE,
  AMBIGUOUS,
  CONFIRMING,
  RESOLVED,
  INVALID_CONFIG
};
struct Resolution {
  ResolveStatus status = ResolveStatus::WAIT_SENSORS;
  loc::MapKind kind = loc::MapKind::UNKNOWN;
  loc::Cell cell{};
  float x_m = 0, y_m = 0, yaw_rad = 0, score = 0, margin = 0;
  int64_t heading_us = 0;
  unsigned confirmations = 0;
};
/** @brief Create a configurable rotation hypothesis set from the supplied Q1
 * map.
 * @returns Default maps and startup POIs; color calibration is disabled.
 * @note Rotations are hypotheses, not a claim about the actual Q2..Q4 layouts.
 */
ResolverConfig default_resolver_config();
static_assert(std::is_trivially_copyable<BootstrapFrame>::value,
              "RTOS snapshot");
} // namespace nav
