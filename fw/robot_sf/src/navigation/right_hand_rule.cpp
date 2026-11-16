// SPDX-License-Identifier: GPL-3.0-only

#include "navigation/right_hand_rule.hpp"
#include "localization/config.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/turn_clearance.hpp"
#include <algorithm>
#include <cmath>
namespace nav {
namespace {
constexpr float kCenterPassM = .002f, kCenterWindowM = .010f;
constexpr float kPivotMarginM = .002f;
constexpr float kEarlyPivotToleranceM = .005f;
bool fresh_stamp(int64_t stamp, int64_t now, int64_t age) {
  return stamp > 0 && stamp <= now && now - stamp <= age;
}
} // namespace
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
  phase_ = config_valid() ? (motion_test::rolling_turns ? FallbackPhase::DECIDE
                                                        : FallbackPhase::COAST)
                          : FallbackPhase::FAULT;
  turn_start_ = last_imu_ = last_update_ = coast_start_ = clear_elapsed_ = 0;
  angle_ = target_ = 0;
  pending_turn_ = 0;
  center_source_ = CenterSource::NONE;
  center_range_us_ = center_confirm_us_ = 0;
  center_confirmations_ = 0;
  center_range_invalid_ = false;
  center_reached_ = false;
  mapped_pivot_authorized_ = false;
}
void RightHandRule::begin_uturn() {
  begin();
  if (phase_ == FallbackPhase::FAULT)
    return;
  // The caller must first prove full swept-circle clearance against every
  // surviving mapped hypothesis. CENTER still independently confirms fresh
  // range/gyro evidence and the measured radial clearances.
  mapped_pivot_authorized_ = true;
  approach_center(loc::kPi);
}
void RightHandRule::cancel() {
  phase_ = FallbackPhase::IDLE;
  pending_turn_ = 0;
  center_source_ = CenterSource::NONE;
  coast_start_ = 0;
  mapped_pivot_authorized_ = false;
}
void RightHandRule::turn(float target, int64_t now, int64_t stamp) {
  phase_ = FallbackPhase::TURN;
  target_ = target;
  angle_ = 0;
  turn_start_ = now;
  last_imu_ = stamp;
}
void RightHandRule::approach_center(float target) {
  phase_ = FallbackPhase::CENTER;
  pending_turn_ = target;
  center_source_ = CenterSource::NONE;
  center_range_us_ = center_confirm_us_ = 0;
  center_confirmations_ = 0;
  coast_start_ = 0;
  center_range_invalid_ = false;
  center_reached_ = false;
}
bool RightHandRule::center_observation(const BootstrapFrame &f,
                                       const loc::PoseSnapshot *p,
                                       const HeadingObservation *heading,
                                       int64_t now, float &remaining,
                                       float &yaw, int64_t &stamp) {
  const bool pose_ok = p && p->tracking && p->grid_confident &&
                       fresh_stamp(p->timestamp_us, now, 150000) &&
                       loc::valid_cell({p->grid_x, p->grid_y}) &&
                       std::isfinite(p->x_m) && std::isfinite(p->y_m) &&
                       std::isfinite(p->yaw_rad) &&
                       p->x_m >= p->grid_x * loc::kCellM &&
                       p->x_m < (p->grid_x + 1) * loc::kCellM &&
                       p->y_m >= p->grid_y * loc::kCellM &&
                       p->y_m < (p->grid_y + 1) * loc::kCellM;
  if (heading && heading->valid &&
      fresh_stamp(heading->timestamp_us, now, 100000) &&
      std::isfinite(heading->yaw_rad))
    yaw = loc::wrap_pi(heading->yaw_rad);
  else if (pose_ok)
    yaw = loc::wrap_pi(p->yaw_rad);
  else
    return false;
  if (center_source_ == CenterSource::RANGE && !center_range_us_ && pose_ok)
    center_source_ = CenterSource::NONE;
  if (center_source_ == CenterSource::NONE)
    center_yaw_ = std::round(yaw / (loc::kPi / 2)) * loc::kPi / 2;
  const float error = loc::wrap_pi(yaw - center_yaw_);
  if (std::fabs(error) > loc::deg2rad(10))
    return false;
  const int h = (int(std::lround(center_yaw_ / (loc::kPi / 2))) + 4) % 4;
  const loc::Cell direction = h == 0   ? loc::Cell{1, 0}
                              : h == 1 ? loc::Cell{0, 1}
                              : h == 2 ? loc::Cell{-1, 0}
                                       : loc::Cell{0, -1};
  if (center_source_ == CenterSource::NONE) {
    if (pose_ok) {
      loc::Cell cell{p->grid_x, p->grid_y};
      const float position = direction.x * p->x_m + direction.y * p->y_m;
      float target =
          (direction.x * (cell.x + .5f) + direction.y * (cell.y + .5f)) *
          loc::kCellM;
      if (target - position < -kCenterWindowM) {
        cell.x += direction.x;
        cell.y += direction.y;
        target += loc::kCellM;
      }
      if (!loc::valid_cell(cell))
        return false;
      center_target_ = target + kCenterPassM;
      center_source_ = CenterSource::POSE;
    } else if (center_range_invalid_)
      return false;
    else
      center_source_ = CenterSource::RANGE;
  }
  if (center_source_ == CenterSource::POSE) {
    if (!pose_ok)
      return false;
    remaining = center_target_ - direction.x * p->x_m - direction.y * p->y_m;
    stamp = p->timestamp_us;
    return true;
  }
  // A front wall lies on a grid line. Project its measured distance, including
  // the sensor offset, onto the travel axis; its phase within the cell pitch
  // locates the next center without trusting a map variant or nominal speed.
  if (std::fabs(error) > loc::deg2rad(5))
    return false;
  const auto &mount = loc::cfg::lidar_mounts[1];
  const float cs = std::cos(yaw), sn = std::sin(yaw);
  const float ox = cs * mount.x_body_m - sn * mount.y_body_m,
              oy = sn * mount.x_body_m + cs * mount.y_body_m;
  const float projection = direction.x * std::cos(yaw + mount.yaw_body_rad) +
                           direction.y * std::sin(yaw + mount.yaw_body_rad);
  const float distance =
      direction.x * ox + direction.y * oy + projection * f.range_m[1];
  if (!std::isfinite(distance) || distance <= 0 ||
      distance > loc::kMazeN * loc::kCellM)
    return false;
  stamp = f.tof_us[1];
  if (center_range_us_ &&
      (stamp < center_range_us_ ||
       (stamp > center_range_us_ &&
        std::fabs(distance - center_last_range_m_) >
            .020f + .1f * float(stamp - center_range_us_) * 1e-6f))) {
    // A changed wall return cannot prove progress towards the latched center.
    // Wait for a qualified pose instead of treating that range jump as travel.
    center_source_ = CenterSource::NONE;
    center_range_invalid_ = true;
    return false;
  }
  if (!center_range_us_) {
    float to_center = std::fmod(distance, loc::kCellM) - loc::kCellM / 2;
    if (to_center < -kCenterWindowM)
      to_center += loc::kCellM;
    center_target_ = distance - to_center - kCenterPassM;
    if (center_target_ < robot::geometry::footprint_radius_m + kPivotMarginM) {
      center_source_ = CenterSource::NONE;
      return false;
    }
  }
  center_last_range_m_ = distance;
  center_range_us_ = stamp;
  remaining = distance - center_target_;
  return true;
}
DriveRequest RightHandRule::update(const BootstrapFrame &f,
                                   const motor::MotorSnapshot &m, int64_t now,
                                   const loc::PoseSnapshot *pose,
                                   const HeadingObservation *heading,
                                   bool turn_inhibited, bool pivot_authorized) {
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
  if (dt > 50000)
    coast_start_ = 0;
  // The task-owned guard has its own bounded correction window. Consume
  // every gyro frame during that window, but charge the turn deadline only
  // for time when RHR owns the pivot. The absolute run deadline is unchanged.
  if (phase_ == FallbackPhase::TURN && turn_inhibited)
    turn_start_ += dt;
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
    center_confirmations_ = 0;
    last_imu_ = 0;
    out.status = ExecuteStatus::PAUSED;
    return out;
  }
  if (m.duty_left != 0 || m.duty_right != 0 || std::fabs(f.imu.gz_rps) > .05f)
    coast_start_ = 0;
  if (phase_ == FallbackPhase::PAUSED) {
    phase_ = pending_turn_ != 0           ? FallbackPhase::CENTER
             : motion_test::rolling_turns ? FallbackPhase::DECIDE
                                          : FallbackPhase::COAST;
    if (pending_turn_ != 0)
      coast_start_ = 0;
  }
  if (!motion_test::enabled && !turn_inhibited && !pivot_authorized &&
      (f.range_m[0] <= cfg_.side_stop_m || f.range_m[2] <= cfg_.side_stop_m)) {
    // The task-owned guard checks the full footprint while it owns recovery.
    // RHR consumes gyro then, but must not fault its pending turn on the
    // guard's temporary lean or its authorized swept-footprint pivot.
    // Outside that ownership, retain the side stop.
    if (phase_ == FallbackPhase::TURN)
      return fault();
    phase_ = FallbackPhase::PAUSED;
    // A side return below the minimum still stops this frame. Fresh quiet
    // motor/gyro evidence nevertheless proves coast across noisy side-stop
    // samples, so the next valid observation can resume away from the wall.
    if (pending_turn_ == 0 && m.duty_left == 0 && m.duty_right == 0 &&
        std::fabs(f.imu.gz_rps) <= .05f) {
      if (!coast_start_)
        coast_start_ = now;
    } else
      coast_start_ = 0;
    center_confirmations_ = 0;
    out.status = ExecuteStatus::PAUSED;
    return out;
  }
  if (motion_test::enabled && phase_ == FallbackPhase::CENTER) {
    const float target = pending_turn_;
    pending_turn_ = 0;
    turn(target, now, f.imu_us);
  }
  if (phase_ == FallbackPhase::CENTER) {
    float remaining = 0, yaw = 0;
    int64_t stamp = 0;
    if (!center_observation(f, pose, heading, now, remaining, yaw, stamp) ||
        remaining < -kCenterWindowM) {
      center_reached_ = false;
      coast_start_ = 0;
      center_confirmations_ = 0;
      out.status = ExecuteStatus::PAUSED;
      return out;
    }
    const float heading_error = loc::wrap_pi(yaw - center_yaw_);
    const float pivot_radius =
        robot::geometry::footprint_radius_m + kPivotMarginM;
    const auto &front_mount = loc::cfg::lidar_mounts[1];
    const float front_clearance =
        cardinal_wall_clearance(f.range_m[1], front_mount, heading_error);
    // Center passage is a reference for approaching a blocked-front pivot,
    // not a required pose. If every measured radial clearance already
    // contains the chassis swept diagonal, pivot where the robot is.
    bool pivot_clear = true;
    for (unsigned i = 0; i < 3; ++i) {
      const auto &mount = loc::cfg::lidar_mounts[i];
      const float clearance =
          cardinal_wall_clearance(f.range_m[i], mount, heading_error);
      pivot_clear =
          pivot_clear && std::isfinite(clearance) && clearance > pivot_radius;
    }
    pivot_clear = pivot_clear || pivot_authorized;
    if (remaining > 0 &&
        (remaining <= kEarlyPivotToleranceM || mapped_pivot_authorized_) &&
        pivot_clear)
      remaining = 0;
    if (remaining <= 0)
      center_reached_ = true;
    if (remaining > kCenterWindowM)
      center_reached_ = false;
    if (center_reached_)
      remaining = 0;
    if (remaining > 0 && remaining <= kEarlyPivotToleranceM && !pivot_clear) {
      out.status = ExecuteStatus::PAUSED;
      out.diagnostics.decision = ExecuteDecision::WAIT_TURN_CLEARANCE;
      out.diagnostics.pivot_angle_rad = pending_turn_;
      out.diagnostics.pivot_angle_valid = true;
      return out;
    }
    if (remaining > 0) {
      coast_start_ = 0;
      center_confirmations_ = 0;
      // Approach the center only while measured front clearance permits the
      // chassis swept diagonal to turn there. Stop if that safe pivot envelope
      // is unavailable; nominal speed is never used to infer clearance.
      if (front_clearance <= pivot_radius) {
        out.status = ExecuteStatus::PAUSED;
        return out;
      }
      out.linear_m_s =
          std::min(cfg_.speed_m_s, std::clamp(1.6f * remaining, .003f, .030f));
      out.angular_rad_s =
          std::clamp(2 * loc::wrap_pi(center_yaw_ - yaw),
                     -cfg_.max_wall_turn_rad_s, cfg_.max_wall_turn_rad_s);
      return out;
    }
    if constexpr (!motion_test::rolling_turns) {
      if (stamp > center_confirm_us_) {
        center_confirm_us_ = stamp;
        center_confirmations_ = std::min(center_confirmations_ + 1, 3U);
      }
      if (m.duty_left != 0 || m.duty_right != 0 ||
          std::fabs(f.imu.gz_rps) > .05f) {
        coast_start_ = 0;
        return out;
      }
      if (!coast_start_)
        coast_start_ = now;
      if (now - coast_start_ < cfg_.coast_us || center_confirmations_ < 3)
        return out;
    }
    if ((pending_turn_ < 0 && f.range_m[2] < cfg_.opening_m) ||
        (pending_turn_ == loc::kPi / 2 && f.range_m[0] < cfg_.opening_m)) {
      // The side seen at the opening edge may no longer be open at the center.
      pending_turn_ = 0;
      phase_ = FallbackPhase::DECIDE;
      return out;
    }
    if (!pivot_clear) {
      out.status = ExecuteStatus::PAUSED;
      // Let the task-owned guard correct lateral posture at the measured
      // center. Otherwise CENTER waits forever for clearance that only the
      // guard can restore, without ever issuing a pivot for it to intercept.
      out.diagnostics.decision = ExecuteDecision::WAIT_TURN_CLEARANCE;
      out.diagnostics.pivot_angle_rad = pending_turn_;
      out.diagnostics.pivot_angle_valid = true;
      return out;
    }
    const float target = pending_turn_;
    pending_turn_ = 0;
    turn(target, now, f.imu_us);
    if constexpr (motion_test::rolling_turns)
      return update(f, m, now, pose, heading, turn_inhibited, pivot_authorized);
    return out;
  }
  if (phase_ == FallbackPhase::COAST) {
    if (m.duty_left == 0 && m.duty_right == 0 &&
        std::fabs(f.imu.gz_rps) <= .05f) {
      if (!coast_start_)
        coast_start_ = now;
      if (now - coast_start_ >= cfg_.coast_us) {
        phase_ = FallbackPhase::DECIDE;
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
    if (!turn_inhibited &&
        ((target_ < 0 && angle_ <= target_ + loc::deg2rad(5)) ||
         (target_ > 0 && angle_ >= target_ - loc::deg2rad(5)))) {
      // Mark a committed exit; suppress a second right turn in the same
      // opening.
      phase_ = FallbackPhase::CLEAR;
      clear_elapsed_ = 0;
      coast_start_ = motion_test::rolling_turns ? 0 : now;
      if constexpr (motion_test::rolling_turns)
        return update(f, m, now, pose, heading, turn_inhibited,
                      pivot_authorized);
      return out;
    }
    out.diagnostics.pivot_angle_rad = target_ - angle_;
    out.diagnostics.pivot_angle_valid = true;
    out.angular_rad_s = std::copysign(cfg_.turn_rad_s, target_);
    if constexpr (motion_test::rolling_turns)
      out.linear_m_s = motion_test::turn_feed_m_s;
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
    if (robot::geometry::front_edge_range(f.range_m[1]) <= cfg_.front_stop_m) {
      phase_ = FallbackPhase::DECIDE;
      if constexpr (motion_test::rolling_turns)
        return update(f, m, now, pose, heading, turn_inhibited,
                      pivot_authorized);
      return out;
    }
    if (dt <= 50000)
      clear_elapsed_ += dt;
    if (clear_elapsed_ < cfg_.clear_us) {
      out.linear_m_s = cfg_.speed_m_s;
      if (heading && heading->valid &&
          fresh_stamp(heading->timestamp_us, now, cfg_.imu_max_age_us) &&
          std::isfinite(heading->yaw_rad)) {
        const float cardinal =
            std::round(heading->yaw_rad / (loc::kPi / 2)) * (loc::kPi / 2);
        // CLEAR suppresses another opening decision, not wall centering.
        // The rear axle leaves the chassis laterally displaced after a turn.
        float offset = 0;
        if (f.range_m[2] < cfg_.opening_m) {
          const float target =
              f.range_m[0] < cfg_.opening_m
                  ? std::min(cfg_.wall_distance_m,
                             .5f * (f.range_m[0] + f.range_m[2]))
                  : cfg_.wall_distance_m;
          offset = target - f.range_m[2];
        } else if (f.range_m[0] < cfg_.opening_m) {
          offset =
              f.range_m[0] - (loc::kCellM / 2 - loc::cfg::platform_half_side_m);
        }
        const float course = std::clamp(std::atan2(offset, .080f),
                                        -loc::deg2rad(10), loc::deg2rad(10));
        out.angular_rad_s =
            std::clamp(2 * loc::wrap_pi(cardinal + course - heading->yaw_rad),
                       -cfg_.max_wall_turn_rad_s, cfg_.max_wall_turn_rad_s);
      }
      return out;
    }
    phase_ = FallbackPhase::FOLLOW;
  }
  if (phase_ == FallbackPhase::DECIDE || phase_ == FallbackPhase::FOLLOW) {
    // RIGHT -> FRONT -> LEFT -> U-turn. Test modes omit turn coasting.
    if (f.range_m[2] >= cfg_.opening_m) {
      if (!motion_test::rolling_turns &&
          (m.duty_left != 0 || m.duty_right != 0 ||
           std::fabs(f.imu.gz_rps) > .05f)) {
        phase_ = FallbackPhase::COAST;
        coast_start_ = 0;
        return out;
      }
      approach_center(-loc::kPi / 2);
      if constexpr (motion_test::rolling_turns)
        return update(f, m, now, pose, heading, turn_inhibited,
                      pivot_authorized);
    } else if (robot::geometry::front_edge_range(f.range_m[1]) >
               cfg_.front_stop_m) {
      phase_ = FallbackPhase::FOLLOW;
      out.linear_m_s = cfg_.speed_m_s;
      // Both walls in a narrow corridor cannot satisfy an independent
      // right-side offset larger than half the free width. Center that gap
      // instead of driving the left sensor into its stop threshold.
      const float wall_distance =
          f.range_m[0] < cfg_.opening_m
              ? std::min(cfg_.wall_distance_m,
                         .5f * (f.range_m[0] + f.range_m[2]))
              : cfg_.wall_distance_m;
      float steering = cfg_.wall_gain * (wall_distance - f.range_m[2]);
      if (heading && heading->valid &&
          fresh_stamp(heading->timestamp_us, now, cfg_.imu_max_age_us) &&
          std::isfinite(heading->yaw_rad)) {
        const float cardinal =
            std::round(heading->yaw_rad / (loc::kPi / 2)) * (loc::kPi / 2);
        // A rear-axle pivot leaves lateral chassis displacement. Heading
        // damping alone must not cancel wall correction before that offset
        // is recovered. Aim toward the measured wall offset over a bounded
        // lookahead, retaining the existing yaw-rate and side-stop limits.
        const float course =
            std::clamp(std::atan2(wall_distance - f.range_m[2], .080f),
                       -loc::deg2rad(10), loc::deg2rad(10));
        steering = 2 * loc::wrap_pi(cardinal + course - heading->yaw_rad);
      }
      out.angular_rad_s = std::clamp(steering, -cfg_.max_wall_turn_rad_s,
                                     cfg_.max_wall_turn_rad_s);
    } else {
      // First stop/coast after a forward leg. The next DECIDE may pivot.
      if (!motion_test::rolling_turns &&
          (m.duty_left != 0 || m.duty_right != 0 ||
           std::fabs(f.imu.gz_rps) > .05f)) {
        phase_ = FallbackPhase::COAST;
        coast_start_ = 0;
        return out;
      }
      approach_center(f.range_m[0] >= cfg_.opening_m ? loc::kPi / 2 : loc::kPi);
      if constexpr (motion_test::rolling_turns)
        return update(f, m, now, pose, heading, turn_inhibited,
                      pivot_authorized);
    }
  }
  return out;
}
} // namespace nav
