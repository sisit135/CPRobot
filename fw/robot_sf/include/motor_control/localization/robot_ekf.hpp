// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "localization/maze_map.hpp"
#include "localization/types.hpp"

namespace loc {
// Platform-independent, no FreeRTOS includes or heap allocation.
class RobotEKF {
public:
  static constexpr int N = 4;
  enum Index { X, Y, YAW, BGZ };
  /** @brief Construct the EKF using cfg::initial_* and default covariance.
   * @returns No value; the new instance is initialized from configuration.
   */
  RobotEKF();
  /** @brief Reset pose, gyro bias, and covariance.
   * @param[in] x Robot-center world X in meters; nonfinite input uses default.
   * @param[in] y Robot-center world Y in meters; nonfinite input uses default.
   * @param[in] position_sigma_m Prior position standard deviation in meters.
   * @param[in] yaw_sigma_rad Prior heading standard deviation in radians.
   * @param[in] yaw CCW heading from world +X in radians; wrapped to [-pi,pi].
   * @returns No value.
   */
  void reset(float x, float y, float yaw, float position_sigma_m = 0.15f,
             float yaw_sigma_rad = deg2rad(25));
  /** @brief Propagate the planar state and covariance using one held input.
   * @param[in] imu Calibrated body gz in rad/s; acceleration is ignored.
   * @param[in] dt_s Positive elapsed seconds, at most 0.05.
   * @returns true on prediction, false for invalid/nonfinite inputs or dt.
   */
  bool predict(const ImuSample &imu, float dt_s);
  /** @brief Inflate uncertainty after missing heading input.
   * @param[in] elapsed_s Duration of the unobserved interval in seconds.
   * @returns No value. Pose mean is retained; covariance is inflated.
   */
  void timing_gap(float elapsed_s);
  /** @brief Correct yaw from a calibrated, approximately horizontal field.
   * @param[in] mag Body-frame magnetic field in uT.
   * @returns true if the field check and innovation gate accept the sample.
   * @note Uses atan2(-my,mx)+cfg::mag_heading_offset_rad for +Y-left axes.
   */
  bool update_mag(const MagSample &mag);
  /** @brief Correct pose using a mapped-wall range measurement.
   * @param[in] range_m Fresh corrected range in meters.
   * @param[in] mount Sensor origin and beam heading relative to robot center.
   * @param[out] observed_axis Optional output: X or Y wall-normal axis on
   * acceptance.
   * @returns true if geometry, range limits, and innovation gate accept it.
   * @note Rejects unknown edges and ambiguous wall endpoints/corners.
   */
  bool update_lidar(float range_m, const LidarMount &mount,
                    int *observed_axis = nullptr);
  /** @brief Apply a mapped TCS34725 cell or grid-stripe event.
   * @param[in] marker Map-associated geometry, indices, and classification
   * confidence.
   * @returns true if consistent or corrected; false for invalid input or
   *          rejected innovation. A failed X/Y correction is rolled back.
   * @note Consistent in-cell positions are not snapped to the cell center.
   *       Corrections outside the interval use a Gaussian approximation.
   */
  bool update_marker(const MarkerSample &marker);
  /** @brief Export pose mean, standard deviations, and cell classification.
   * @returns A snapshot without temporal metadata or tracking status.
   */
  PoseSnapshot pose() const;
  /** @brief Inspect one state component for diagnostics/tests.
   * @param[in] i Valid Index in [0,N); no runtime bounds checking.
   * @returns State value in meters, radians, or rad/s for that component.
   */
  float state(int i) const { return x_[i]; }
  /** @brief Inspect a covariance entry for diagnostics/tests.
   * @param[in] i Row index in [0,N).
   * @param[in] j Column index in [0,N).
   * @returns Covariance in the product of the corresponding state units.
   */
  float covariance(int i, int j) const { return P_[i][j]; }
  /** @brief Ray cast against the supplied Q1 mapped walls.
   * @param[in] x Robot-center world X in meters.
   * @param[in] y Robot-center world Y in meters.
   * @param[in] yaw Robot CCW heading in radians.
   * @param[in] mount Sensor extrinsics in the robot body frame.
   * @returns Expected meters, or NaN for nonfinite inputs/outside sensor
   * origin.
   */
  static float expected_range(float x, float y, float yaw,
                              const LidarMount &mount);
  /** @brief Trace a specific runtime map without modifying the filter.
   * @param[in] map Complete or partial maze in the current world frame.
   * @param[in] x World X in meters.
   * @param[in] y World Y in meters.
   * @param[in] position_sigma_m Prior position standard deviation in meters.
   * @param[in] yaw_sigma_rad Prior heading standard deviation in radians.
   * @param[in] yaw CCW heading in radians.
   * @param[in] mount Sensor extrinsics.
   * @returns First known wall range, or NaN when no valid mapped hit exists.
   */
  static float expected_range(const MazeMap &map, float x, float y, float yaw,
                              const LidarMount &mount);
  /** @brief Replace the map snapshot from the fusion-owner task only.
   * @param[in] map Valid world-frame map; does not transform the state.
   * @returns true on copy, false on invalid masks/label.
   */
  bool set_maze(const MazeMap &map);
  const MazeMap &maze() const { return maze_; }

private:
  /** @brief Perform one gated scalar EKF correction in Joseph form.
   * @param[in] z Observed scalar measurement.
   * @param[in] h Predicted measurement at the current state.
   * @param[in] H Four-element measurement Jacobian.
   * @param[in] variance Strictly positive measurement variance.
   * @param[in] angle Whether to wrap the innovation as an angle in radians.
   * @param[in] nis_gate Maximum normalized innovation squared.
   * @returns true if state/covariance were corrected, false if rejected.
   */
  bool update_scalar(float z, float h, const float H[N], float variance,
                     bool angle, float nis_gate);
  MazeMap maze_ = MazeMap::q1();
  float x_[N]{};
  float P_[N][N]{};
};
} // namespace loc
