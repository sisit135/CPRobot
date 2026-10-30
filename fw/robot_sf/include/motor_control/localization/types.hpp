#pragma once
#include <cstdint>
#include <type_traits>

namespace loc {
constexpr float kPi = 3.14159265358979323846f;
/** @brief Convert degrees to radians.
 * @param[in] degrees Angle in degrees.
 * @returns The angle in radians.
 */
constexpr float deg2rad(float degrees) { return degrees * kPi / 180.0f; }
/** @brief Wrap a finite angle into [-pi, pi].
 * @param[in] angle Angle in radians.
 * @returns Wrapped radians, or NaN for nonfinite input.
 */
float wrap_pi(float angle);

// Body axes: +X forward, +Y left, +Z up. World yaw: CCW from map +X.
// Accelerometer channels are diagnostic only; EKF prediction uses gz only.
struct ImuSample {
  float ax_mps2 = 0, ay_mps2 = 0, az_mps2 = 0;
  float gx_rps = 0, gy_rps = 0, gz_rps = 0;
};
struct MagSample {
  float mx_uT = 0, my_uT = 0, mz_uT = 0;
};
enum class TofId : uint8_t { LEFT = 0, FRONT = 1, RIGHT = 2 };
struct LidarMount {
  float x_body_m, y_body_m, yaw_body_rad;
};
enum class MarkerGeometry : uint8_t { CELL, X_LINE, Y_LINE };
struct MarkerSample {
  MarkerGeometry geometry = MarkerGeometry::CELL;
  uint32_t observation_id = 0; // Monotonic debounced entry ID; zero is invalid.
  bool detected = false;
  int gx = -1, gy = -1;
  float confidence = 0;
};
enum class SensorKind : uint8_t { IMU, MAG, TOF, MARKER };

// Queue copies by value. No pointers/references to temporary HAL buffers.
struct SensorEvent {
  SensorKind kind = SensorKind::IMU;
  int64_t timestamp_us = 0;
  ImuSample imu{};
  MagSample mag{};
  bool magnetic_clean = false; // Board confirms motors off and field settled.
  TofId tof_id = TofId::FRONT;
  float range_m = 0;
  MarkerSample marker{};
};

struct FusionStats {
  uint32_t accepted_mag = 0, rejected_mag = 0;
  uint32_t accepted_tof = 0, rejected_tof = 0;
  uint32_t accepted_marker = 0, rejected_marker = 0;
  uint32_t invalid = 0, stale = 0, out_of_order = 0, duplicate = 0;
  uint32_t timing_gaps = 0;
};
enum class FusionHealth : uint8_t {
  INITIALIZING,
  TRACKING,
  DEGRADED,
  FAILED,
  CONTRADICTING
};
struct PoseSnapshot {
  FusionHealth health = FusionHealth::INITIALIZING;
  int64_t timestamp_us = 0; // Filter epoch, not the time it was read.
  int64_t last_imu_us = 0;
  float x_m = 0, y_m = 0, yaw_rad = 0;
  float gyro_bias_rps = 0;
  int64_t last_x_fix_us = 0, last_y_fix_us = 0;
  float sigma_x_m = 0, sigma_y_m = 0, sigma_yaw_rad = 0;
  int grid_x = -1, grid_y = -1; // -1 if pose is outside the map.
  bool tracking = false;
  bool grid_confident = false; // Axis-aligned 2-sigma bounds inside the cell.
  FusionStats stats{};
  uint32_t queue_dropped = 0;
};
static_assert(std::is_trivially_copyable<SensorEvent>::value,
              "FreeRTOS queue item");
static_assert(std::is_trivially_copyable<PoseSnapshot>::value,
              "Snapshot queue item");
} // namespace loc
