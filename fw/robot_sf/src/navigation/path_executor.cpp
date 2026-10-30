#include "navigation/path_executor.hpp"
#include <algorithm>
#include <cmath>
namespace nav {
void PathExecutor::cancel() {
  path_ = {};
  phase_ = ExecuteStatus::IDLE;
  index_ = stable_ = 0;
  zero_since_ = 0;
}
bool PathExecutor::begin(const loc::MazeMap &m, loc::Cell goal,
                         const loc::PoseSnapshot &p, int64_t now) {
  cancel();
  if (!m.valid() || !p.tracking || p.timestamp_us <= 0 ||
      p.timestamp_us > now || now - p.timestamp_us > 150000 ||
      !std::isfinite(p.x_m) || !std::isfinite(p.y_m) ||
      !std::isfinite(p.yaw_rad) || !std::isfinite(cfg_.max_speed_m_s) ||
      cfg_.max_speed_m_s <= 0 || cfg_.max_speed_m_s > 0.2f ||
      !std::isfinite(cfg_.max_turn_rad_s) || cfg_.max_turn_rad_s <= 0 ||
      cfg_.max_turn_rad_s > 2 || !std::isfinite(cfg_.position_tolerance_m) ||
      cfg_.position_tolerance_m <= 0 || cfg_.position_tolerance_m >= 0.05f ||
      !std::isfinite(cfg_.heading_tolerance_rad) ||
      cfg_.heading_tolerance_rad <= 0 ||
      cfg_.heading_tolerance_rad >= loc::kPi / 4 ||
      !std::isfinite(cfg_.obstacle_stop_m) || cfg_.obstacle_stop_m <= 0 ||
      !std::isfinite(cfg_.settle_gyro_rad_s) || cfg_.settle_gyro_rad_s <= 0 ||
      !std::isfinite(cfg_.settle_pose_step_m) || cfg_.settle_pose_step_m <= 0 ||
      cfg_.coast_settle_us <= 0 ||
      cfg_.coast_settle_us >= cfg_.leg_timeout_us || cfg_.leg_timeout_us <= 0) {
    phase_ = ExecuteStatus::FAULT;
    return false;
  }
  AStar::Generator planner;
  planner.setMaze(m);
  path_ = planner.findPath({p.grid_x, p.grid_y}, goal);
  if (path_.empty()) {
    phase_ = ExecuteStatus::NO_PATH;
    return false;
  }
  map_ = m;
  index_ = path_.count > 1 ? 1 : 0;
  phase_ = path_.count > 1 ? ExecuteStatus::TURN : ExecuteStatus::DONE;
  leg_start_ = now;
  last_pose_ = last_motor_ = zero_since_ = 0;
  last_x_ = p.x_m;
  last_y_ = p.y_m;
  return true;
}
DriveRequest PathExecutor::update(const loc::PoseSnapshot &measured,
                                  const motor::MotorSnapshot &m,
                                  const BootstrapFrame &f, int64_t now,
                                  const HeadingObservation *heading) {
  loc::PoseSnapshot p = measured;
  // A pivot can temporarily make wall-ray translation unobservable. Only
  // heading may advance then; never fabricate a translated pose or permit
  // ADVANCE/settled arrival without fresh independent range localization.
  bool heading_turn = phase_ == ExecuteStatus::TURN && heading &&
                      heading->valid && heading->timestamp_us > 0 &&
                      heading->timestamp_us <= now &&
                      now - heading->timestamp_us <= 100000 &&
                      std::isfinite(heading->yaw_rad) && p.timestamp_us > 0;
  if (heading_turn) {
    for (int i = 0; i < 3; ++i)
      if (f.tof_us[i] <= 0 || f.tof_us[i] > now || now - f.tof_us[i] > 150000 ||
          !std::isfinite(f.range_m[i]) || f.range_m[i] <= .015f ||
          f.range_m[i] >= 1.5f)
        heading_turn = false;
  }
  if (heading_turn)
    p.yaw_rad = heading->yaw_rad;
  DriveRequest out{};
  out.status = phase_;
  out.waypoint = index_;
  if (phase_ == ExecuteStatus::IDLE || phase_ == ExecuteStatus::DONE ||
      phase_ == ExecuteStatus::NO_PATH || phase_ == ExecuteStatus::BLOCKED ||
      phase_ == ExecuteStatus::FAULT)
    return out;
  auto fault = [&]() {
    phase_ = ExecuteStatus::FAULT;
    out.status = phase_;
    return out;
  };
  if (now < leg_start_ || now - leg_start_ > cfg_.leg_timeout_us)
    return fault();
  if (m.status == motor::ControlStatus::DRIVER_FAULT ||
      m.status == motor::ControlStatus::TIMING_FAULT ||
      m.status == motor::ControlStatus::INVALID_COMMAND ||
      m.status == motor::ControlStatus::COMMAND_TIMEOUT)
    return fault();
  if ((!heading_turn &&
       (!p.tracking || p.timestamp_us <= 0 || p.timestamp_us > now ||
        now - p.timestamp_us > 150000)) ||
      !std::isfinite(p.x_m) || !std::isfinite(p.y_m) ||
      !std::isfinite(p.yaw_rad) || m.timestamp_us <= 0 ||
      m.timestamp_us > now || now - m.timestamp_us > 50000 ||
      !std::isfinite(m.duty_left) || !std::isfinite(m.duty_right) ||
      std::fabs(m.duty_left) > 1 || std::fabs(m.duty_right) > 1) {
    stable_ = 0;
    zero_since_ = 0;
    out.status = ExecuteStatus::PAUSED;
    return out;
  }
  const auto target = path_[index_], previous = path_[index_ - 1];
  if (!map_.can_step(previous, target)) {
    phase_ = ExecuteStatus::BLOCKED;
    out.status = phase_;
    return out;
  }
  const loc::Cell current{p.grid_x, p.grid_y};
  if (current != previous && current != target)
    return fault();
  const float dx = (target.x + 0.5f) * loc::kCellM - p.x_m,
              dy = (target.y + 0.5f) * loc::kCellM - p.y_m;
  const float distance = std::hypot(dx, dy);
  const float desired =
      std::atan2(float(target.y - previous.y), float(target.x - previous.x));
  const float error = loc::wrap_pi(desired - p.yaw_rad);
  const bool fresh_pose = p.tracking && p.timestamp_us <= now &&
                          now - p.timestamp_us <= 150000 &&
                          p.timestamp_us > last_pose_,
             fresh_motor = m.timestamp_us > last_motor_;
  const bool quiet = f.imu_us > 0 && f.imu_us <= now &&
                     now - f.imu_us <= 100000 && std::isfinite(f.imu.gz_rps) &&
                     std::fabs(f.imu.gz_rps) <= cfg_.settle_gyro_rad_s;
  const bool stable_pose =
      last_pose_ > 0 &&
      std::hypot(p.x_m - last_x_, p.y_m - last_y_) <= cfg_.settle_pose_step_m;
  if (m.duty_left == 0 && m.duty_right == 0 && quiet && stable_pose) {
    if (!zero_since_)
      zero_since_ = now;
  } else
    zero_since_ = 0;
  const bool settled =
      zero_since_ > 0 && now - zero_since_ >= cfg_.coast_settle_us;
  if (fresh_pose) {
    last_x_ = p.x_m;
    last_y_ = p.y_m;
  }
  last_pose_ = p.timestamp_us;
  last_motor_ = m.timestamp_us;
  if (phase_ == ExecuteStatus::TURN) {
    if (std::fabs(error) <= cfg_.heading_tolerance_rad) {
      if (settled && fresh_pose && fresh_motor)
        ++stable_;
      else if (!settled)
        stable_ = 0;
      if (stable_ >= 3) {
        phase_ = ExecuteStatus::ADVANCE;
        stable_ = 0;
      }
    } else {
      stable_ = 0;
      out.angular_rad_s =
          std::clamp(2 * error, -cfg_.max_turn_rad_s, cfg_.max_turn_rad_s);
    }
  } else if (phase_ == ExecuteStatus::ADVANCE) {
    if (distance <= cfg_.position_tolerance_m) {
      phase_ = ExecuteStatus::SETTLE;
      stable_ = 0;
    } else {
      if (std::fabs(error) > loc::deg2rad(20)) {
        phase_ = ExecuteStatus::TURN;
        stable_ = 0;
      } else {
        const float forward = dx * std::cos(desired) + dy * std::sin(desired);
        if (forward <= 0)
          return fault();
        if (f.tof_us[1] <= 0 || f.tof_us[1] > now ||
            now - f.tof_us[1] > 100000 || !std::isfinite(f.range_m[1]) ||
            f.range_m[1] <= 0 || f.range_m[1] >= 1.5f) {
          stable_ = 0;
          zero_since_ = 0;
          out.status = ExecuteStatus::PAUSED;
          return out;
        }
        if (f.range_m[1] < cfg_.obstacle_stop_m) {
          phase_ = ExecuteStatus::BLOCKED;
          out.status = phase_;
          return out;
        }
        out.linear_m_s = std::min(cfg_.max_speed_m_s, forward);
        out.angular_rad_s =
            std::clamp(2 * error, -cfg_.max_turn_rad_s, cfg_.max_turn_rad_s);
      }
    }
  } else if (phase_ == ExecuteStatus::SETTLE) {
    if (distance > 0.05f)
      return fault();
    if (settled && fresh_pose && fresh_motor)
      ++stable_;
    else if (!settled)
      stable_ = 0;
    if (stable_ >= 3) {
      if (index_ + 1 >= path_.count)
        phase_ = ExecuteStatus::DONE;
      else {
        ++index_;
        phase_ = ExecuteStatus::TURN;
        leg_start_ = now;
      }
      stable_ = 0;
    }
  }
  out.status = phase_;
  out.waypoint = index_;
  return out;
}
} // namespace nav
