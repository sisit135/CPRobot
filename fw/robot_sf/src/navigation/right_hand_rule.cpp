#include "navigation/right_hand_rule.hpp"
#include <algorithm>
#include <cmath>
namespace nav {
bool RightHandRule::config_valid() const {
  return std::isfinite(cfg_.speed_m_s) && cfg_.speed_m_s > 0 &&
         cfg_.speed_m_s <= .1f && std::isfinite(cfg_.turn_rad_s) &&
         cfg_.turn_rad_s > 0 && cfg_.turn_rad_s <= 1 &&
         std::isfinite(cfg_.front_stop_m) && cfg_.front_stop_m > 0 &&
         std::isfinite(cfg_.side_stop_m) && cfg_.side_stop_m > 0 &&
         std::isfinite(cfg_.wall_distance_m) &&
         cfg_.wall_distance_m > cfg_.side_stop_m &&
         std::isfinite(cfg_.opening_m) &&
         cfg_.opening_m > cfg_.wall_distance_m &&
         std::isfinite(cfg_.wall_gain) && cfg_.wall_gain >= 0 &&
         std::isfinite(cfg_.max_wall_turn_rad_s) &&
         cfg_.max_wall_turn_rad_s >= 0 && cfg_.max_wall_turn_rad_s <= 1 &&
         cfg_.range_max_age_us > 0 && cfg_.imu_max_age_us > 0 &&
         cfg_.turn_timeout_us > 0 && cfg_.coast_us > 0 && cfg_.clear_us > 0;
}
void RightHandRule::begin() {
  phase_ = config_valid() ? FallbackPhase::COAST : FallbackPhase::FAULT;
  turn_start_ = last_imu_ = last_update_ = coast_start_ = clear_elapsed_ = 0;
  angle_ = target_ = 0;
}
void RightHandRule::turn(float target, int64_t now, int64_t stamp) {
  phase_ = FallbackPhase::TURN;
  target_ = target;
  angle_ = 0;
  turn_start_ = now;
  last_imu_ = stamp;
}
DriveRequest RightHandRule::update(const BootstrapFrame &f,
                                   const motor::MotorSnapshot &m, int64_t now) {
  DriveRequest out{};
  out.status = ExecuteStatus::FALLBACK;
  if (phase_ == FallbackPhase::IDLE) {
    out.status = ExecuteStatus::IDLE;
    return out;
  }
  if (phase_ == FallbackPhase::FAULT) {
    out.status = ExecuteStatus::FAULT;
    return out;
  }
  auto fault = [&]() {
    phase_ = FallbackPhase::FAULT;
    out.status = ExecuteStatus::FAULT;
    return out;
  };
  if (now < 0 || (last_update_ && now < last_update_))
    return fault();
  const int64_t dt = last_update_ ? now - last_update_ : 0;
  last_update_ = now;
  if (m.status == motor::ControlStatus::DRIVER_FAULT ||
      m.status == motor::ControlStatus::TIMING_FAULT ||
      m.status == motor::ControlStatus::INVALID_COMMAND ||
      m.status == motor::ControlStatus::COMMAND_TIMEOUT ||
      m.status == motor::ControlStatus::RUN_TIMEOUT ||
      m.status == motor::ControlStatus::HALTED ||
      m.status == motor::ControlStatus::GOAL_REACHED)
    return fault();
  if (phase_ == FallbackPhase::TURN &&
      (now < turn_start_ || now - turn_start_ >= cfg_.turn_timeout_us))
    return fault();
  auto fresh = [&](int64_t stamp, int64_t age) {
    return stamp > 0 && stamp <= now && now - stamp <= age;
  };
  bool sensors_ok = fresh(f.imu_us, cfg_.imu_max_age_us) &&
                    std::isfinite(f.imu.gz_rps) &&
                    std::fabs(f.imu.gz_rps) <= 30;
  for (unsigned i = 0; i < 3; ++i)
    sensors_ok = sensors_ok && fresh(f.tof_us[i], cfg_.range_max_age_us) &&
                 std::isfinite(f.range_m[i]) && f.range_m[i] > 0 &&
                 f.range_m[i] < 2.f;
  const bool motor_ok =
      fresh(m.timestamp_us, 50000) && std::isfinite(m.duty_left) &&
      std::isfinite(m.duty_right) && std::fabs(m.duty_left) <= 1 &&
      std::fabs(m.duty_right) <= 1;
  if (!sensors_ok || !motor_ok) {
    // A turn cannot be reconstructed across an IMU outage. Stop and require a
    // new run.
    if (phase_ == FallbackPhase::TURN)
      return fault();
    phase_ = FallbackPhase::PAUSED;
    coast_start_ = clear_elapsed_ = 0;
    last_imu_ = 0;
    out.status = ExecuteStatus::PAUSED;
    return out;
  }
  if (phase_ == FallbackPhase::PAUSED) {
    phase_ = FallbackPhase::COAST;
    coast_start_ = 0;
  }
  if (f.range_m[0] <= cfg_.side_stop_m || f.range_m[2] <= cfg_.side_stop_m) {
    // Close side obstacles prohibit pivoting as well as translation.
    if (phase_ == FallbackPhase::TURN)
      return fault();
    phase_ = FallbackPhase::PAUSED;
    coast_start_ = 0;
    out.status = ExecuteStatus::PAUSED;
    return out;
  }
  if (phase_ == FallbackPhase::COAST) {
    if (m.duty_left == 0 && m.duty_right == 0 &&
        std::fabs(f.imu.gz_rps) <= .05f) {
      if (!coast_start_)
        coast_start_ = now;
      if (now - coast_start_ >= cfg_.coast_us) {
        phase_ = FallbackPhase::DECIDE;
        coast_start_ = 0;
      }
    } else
      coast_start_ = 0;
    return out;
  }
  if (phase_ == FallbackPhase::TURN) {
    if (f.imu_us < last_imu_)
      return fault();
    if (f.imu_us > last_imu_) {
      const int64_t gap = f.imu_us - last_imu_;
      if (gap > 50000)
        return fault();
      angle_ += f.imu.gz_rps * float(gap) * 1e-6f;
      last_imu_ = f.imu_us;
    }
    if ((target_ < 0 && angle_ <= target_ + loc::deg2rad(5)) ||
        (target_ > 0 && angle_ >= target_ - loc::deg2rad(5))) {
      // Mark a committed exit; suppress a second right turn in the same
      // opening.
      phase_ = FallbackPhase::CLEAR;
      clear_elapsed_ = 0;
      coast_start_ = now;
      return out;
    }
    out.angular_rad_s = std::copysign(cfg_.turn_rad_s, target_);
    return out;
  }
  if (phase_ == FallbackPhase::CLEAR && coast_start_) {
    if (m.duty_left != 0 || m.duty_right != 0 || std::fabs(f.imu.gz_rps) > .05f)
      coast_start_ = now;
    if (now - coast_start_ < cfg_.coast_us)
      return out;
    coast_start_ = 0;
    return out;
  }
  if (phase_ == FallbackPhase::CLEAR) {
    if (f.range_m[1] <= cfg_.front_stop_m) {
      phase_ = FallbackPhase::DECIDE;
      return out;
    }
    if (dt <= 50000)
      clear_elapsed_ += dt;
    if (clear_elapsed_ < cfg_.clear_us) {
      out.linear_m_s = cfg_.speed_m_s;
      return out;
    }
    phase_ = FallbackPhase::FOLLOW;
  }
  if (phase_ == FallbackPhase::DECIDE || phase_ == FallbackPhase::FOLLOW) {
    // RIGHT -> FRONT -> LEFT -> U-turn, always at zero forward speed for turns.
    if (f.range_m[2] >= cfg_.opening_m) {
      if (m.duty_left != 0 || m.duty_right != 0 ||
          std::fabs(f.imu.gz_rps) > .05f) {
        phase_ = FallbackPhase::COAST;
        coast_start_ = 0;
        return out;
      }
      turn(-loc::kPi / 2, now, f.imu_us);
    } else if (f.range_m[1] > cfg_.front_stop_m) {
      phase_ = FallbackPhase::FOLLOW;
      out.linear_m_s = cfg_.speed_m_s;
      out.angular_rad_s =
          std::clamp(cfg_.wall_gain * (cfg_.wall_distance_m - f.range_m[2]),
                     -cfg_.max_wall_turn_rad_s, cfg_.max_wall_turn_rad_s);
    } else {
      // First stop/coast after a forward leg. The next DECIDE may pivot.
      if (m.duty_left != 0 || m.duty_right != 0 ||
          std::fabs(f.imu.gz_rps) > .05f) {
        phase_ = FallbackPhase::COAST;
        coast_start_ = 0;
        return out;
      }
      turn(f.range_m[0] >= cfg_.opening_m ? loc::kPi / 2 : loc::kPi, now,
           f.imu_us);
    }
  }
  return out;
}
} // namespace nav
