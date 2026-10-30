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
  FAULT
};
struct RightHandConfig {
  float speed_m_s = .03f, turn_rad_s = .5f, wall_distance_m = .04f;
  float front_stop_m = .045f, side_stop_m = .015f, opening_m = .12f;
  float wall_gain = 3.f, max_wall_turn_rad_s = .25f;
  int64_t range_max_age_us = 200000, imu_max_age_us = 100000;
  int64_t turn_timeout_us = 8000000, coast_us = 400000, clear_us = 3000000;
};
/** Reactive fallback: no EKF pose/map or encoder required; fresh body gyro
 * closes turns. Inputs are snapshots produced by the single HAL bus-owner task.
 * No sensor I/O here. Clear duration and optical clearances require physical
 * chassis calibration.
 */
class RightHandRule {
public:
  explicit RightHandRule(const RightHandConfig &config = RightHandConfig{})
      : cfg_(config) {}
  void begin();
  void cancel() { phase_ = FallbackPhase::IDLE; }
  DriveRequest update(const BootstrapFrame &frame,
                      const motor::MotorSnapshot &motor, int64_t now);
  FallbackPhase phase() const { return phase_; }

private:
  RightHandConfig cfg_;
  FallbackPhase phase_ = FallbackPhase::IDLE;
  int64_t turn_start_ = 0, last_imu_ = 0, last_update_ = 0, coast_start_ = 0,
          clear_elapsed_ = 0;
  float angle_ = 0, target_ = 0;
  bool config_valid() const;
  void turn(float target, int64_t now, int64_t imu_stamp);
};
} // namespace nav
