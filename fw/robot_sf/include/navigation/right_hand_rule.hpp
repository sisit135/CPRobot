// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "navigation/path_executor.hpp"
namespace nav {
enum class FallbackPhase : uint8_t {
  IDLE,
  DECIDE,
  TURN,
  COAST,
  CLEAR,
  FOLLOW,
  PAUSED,
  FAULT,
  CENTER
};
struct RightHandConfig {
  float speed_m_s = .10f, turn_rad_s = 1.0f, wall_distance_m = .04f;
  float front_stop_m = .045f, side_stop_m = .015f, opening_m = .12f;
  float wall_gain = 3.f, max_wall_turn_rad_s = .25f;
  int64_t range_max_age_us = 200000, imu_max_age_us = 100000;
  int64_t turn_timeout_us = 8000000, coast_us = 400000, clear_us = 3000000;
};
/** Reactive fallback: no EKF pose/map or encoder required; fresh body gyro
 * closes turns. A side turn first approaches the cell center using a qualified
 * feature pose, or front-wall ranges projected with fresh independent heading.
 * Missing center evidence pauses the pending turn. Inputs are snapshots; no
 * sensor I/O here. Clear duration and optical clearances require calibration.
 */
class RightHandRule {
public:
  explicit RightHandRule(const RightHandConfig &config = RightHandConfig{})
      : cfg_(config) {}
  void begin();
  /** Start a center-qualified 180-degree turn without running DECIDE. */
  void begin_uturn();
  void cancel();
  DriveRequest
  update(const BootstrapFrame &frame, const motor::MotorSnapshot &motor,
         int64_t now, const loc::PoseSnapshot *pose = nullptr,
         const HeadingObservation *heading = nullptr,
         // Only the task-owned full-footprint guard may supply
         // either ownership flag. Sensor/IMU deadlines still apply.
         bool turn_inhibited = false, bool pivot_authorized = false);
  FallbackPhase phase() const { return phase_; }

private:
  RightHandConfig cfg_;
  FallbackPhase phase_ = FallbackPhase::IDLE;
  int64_t turn_start_ = 0, last_imu_ = 0, last_update_ = 0, coast_start_ = 0,
          clear_elapsed_ = 0;
  float angle_ = 0, target_ = 0;
  enum class CenterSource : uint8_t { NONE, POSE, RANGE };
  CenterSource center_source_ = CenterSource::NONE;
  float pending_turn_ = 0, center_yaw_ = 0, center_target_ = 0;
  float center_last_range_m_ = 0;
  int64_t center_range_us_ = 0, center_confirm_us_ = 0;
  unsigned center_confirmations_ = 0;
  bool center_range_invalid_ = false;
  bool center_reached_ = false;
  bool mapped_pivot_authorized_ = false;
  bool config_valid() const;
  void turn(float target, int64_t now, int64_t imu_stamp);
  void approach_center(float target);
  bool center_observation(const BootstrapFrame &, const loc::PoseSnapshot *,
                          const HeadingObservation *, int64_t now,
                          float &remaining_m, float &yaw, int64_t &stamp);
};
} // namespace nav
