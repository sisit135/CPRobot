#pragma once
#include "localization/robot_ekf.hpp"

namespace loc {
// One consumer owns this engine. Portable temporal logic, tested on host.
class FusionEngine {
public:
  /** @brief Install a resolved maze and bootstrap pose before accepting events.
   * @param[in] map Valid world-frame map.
   * @param[in] x_m Initial position mean in meters; cell center is a
   * hypothesis.
   * @param[in] y_m Initial position mean in meters.
   * @param[in] yaw_rad Qualified startup magnetic heading in radians.
   * @param[in] heading_us Timestamp of that magnetic observation.
   * @returns true on valid initialization; false without changing the engine.
   */
  bool initialize(const MazeMap &map, float x_m, float y_m, float yaw_rad,
                  int64_t heading_us);
  /** @brief Validate time ordering, predict, and fuse one sensor event.
   * @param[in] event A fresh queue item with an acquisition timestamp.
   * @param[in] now_us Current monotonic time in the same clock domain.
   * @returns true for an accepted event (including marker absence), false for
   *          invalid, stale, duplicate, delayed, or gated-out measurements.
   * @note A gated-out measurement can still advance the prediction epoch.
   *       Equal timestamps from different sensor streams are permitted.
   */
  bool process(const SensorEvent &event, int64_t now_us);
  /** @brief Export the filter at its last processed event epoch.
   * @param[in] now_us Current time used to evaluate sensor freshness.
   * @returns Pose, temporal validity flags, and accumulated diagnostics.
   * @note Does not extrapolate the pose to now_us.
   */
  PoseSnapshot snapshot(int64_t now_us) const;

private:
  /** @brief Propagate to a new event timestamp using the previous IMU sample.
   * @param[in] timestamp_us Requested filter epoch in microseconds.
   * @returns true if advanced/unchanged, false for reversed time/prediction
   * failure.
   */
  bool advance(int64_t timestamp_us);
  RobotEKF ekf_{};
  ImuSample held_imu_{};
  int64_t epoch_us_ = 0, last_imu_us_ = 0;
  int64_t last_x_fix_us_ = 0, last_y_fix_us_ = 0, last_heading_us_ = 0;
  int64_t last_stream_us_[6]{}; // IMU, MAG, ToF L/F/R, MARKER.
  bool have_epoch_ = false, have_imu_ = false;
  uint32_t last_marker_id_ = 0;
  FusionStats stats_{};
  FusionHealth health_ = FusionHealth::INITIALIZING;
  unsigned contradictory_ranges_[3]{};
  unsigned contradictory_markers_ = 0;
};
} // namespace loc
