// SPDX-License-Identifier: GPL-3.0-only

#include "navigation/path_executor.hpp"
#include "localization/config.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/turn_clearance.hpp"
#include <algorithm>
#include <cmath>
namespace nav {
// A circle enclosing the square chassis bounds every heading of a pivot,
// including a 180-degree U-turn. Check finite wall spans so an open branch can
// permit a turn before the mathematical center of its cell.
float mapped_pivot_clearance(const loc::MazeMap &map, float x, float y) {
  if (!map.valid() || !std::isfinite(x) || !std::isfinite(y) || x < 0 ||
      y < 0 || x > loc::kMazeN * loc::kCellM || y > loc::kMazeN * loc::kCellM)
    return 0;
  float nearest_squared = 4.f;
  auto segment = [&](bool vertical, float line, float start) {
    const float normal = (vertical ? x : y) - line;
    const float along = vertical ? y : x;
    const float tangent = along - std::clamp(along, start, start + loc::kCellM);
    nearest_squared =
        std::min(nearest_squared, normal * normal + tangent * tangent);
  };
  for (int cy = 0; cy < loc::kMazeN; ++cy)
    for (int cx = 0; cx < loc::kMazeN; ++cx) {
      const loc::Cell cell{cx, cy};
      if (cx == loc::kMazeN - 1 ||
          map.edge(cell, loc::EAST) != loc::EdgeState::OPEN)
        segment(true, (cx + 1) * loc::kCellM, cy * loc::kCellM);
      if (cy == loc::kMazeN - 1 ||
          map.edge(cell, loc::NORTH) != loc::EdgeState::OPEN)
        segment(false, (cy + 1) * loc::kCellM, cx * loc::kCellM);
      if (cx == 0)
        segment(true, 0, cy * loc::kCellM);
      if (cy == 0)
        segment(false, 0, cx * loc::kCellM);
    }
  return std::sqrt(nearest_squared);
}
namespace {
constexpr float kPivotRadiusM = robot::geometry::footprint_radius_m;
constexpr float kPivotMarginM = .002f;
// Keep a positive wall margin after settling while allowing 1 mm of range
// jitter without repeatedly restarting the approach.
constexpr float kPivotHoldMarginM = .001f;
} // namespace
const char *execute_fault_name(ExecuteFault fault) {
  switch (fault) {
  case ExecuteFault::NONE:
    return "NONE";
  case ExecuteFault::INVALID_START:
    return "INVALID_START";
  case ExecuteFault::LEG_TIMEOUT:
    return "LEG_TIMEOUT";
  case ExecuteFault::MOTOR_FAULT:
    return "MOTOR_FAULT";
  case ExecuteFault::UNEXPECTED_CELL:
    return "UNEXPECTED_CELL";
  case ExecuteFault::TARGET_PASSED:
    return "TARGET_PASSED";
  case ExecuteFault::SETTLE_DRIFT:
    return "SETTLE_DRIFT";
  }
  return "UNKNOWN";
}
const char *execute_decision_name(ExecuteDecision decision) {
  switch (decision) {
  case ExecuteDecision::INACTIVE:
    return "INACTIVE";
  case ExecuteDecision::TURN_TO_HEADING:
    return "TURN_TO_HEADING";
  case ExecuteDecision::WAIT_TURN_SETTLE:
    return "WAIT_TURN_SETTLE";
  case ExecuteDecision::ADVANCE:
    return "ADVANCE";
  case ExecuteDecision::APPROACH_CENTER:
    return "APPROACH_CENTER";
  case ExecuteDecision::ARRIVAL_STOP:
    return "ARRIVAL_STOP";
  case ExecuteDecision::WAIT_ARRIVAL_SETTLE:
    return "WAIT_ARRIVAL_SETTLE";
  case ExecuteDecision::REAPPROACH_CENTER:
    return "REAPPROACH_CENTER";
  case ExecuteDecision::TURN_COMPLETE:
    return "TURN_COMPLETE";
  case ExecuteDecision::NEXT_WAYPOINT:
    return "NEXT_WAYPOINT";
  case ExecuteDecision::PASS_WAYPOINT:
    return "PASS_WAYPOINT";
  case ExecuteDecision::ARRIVED:
    return "ARRIVED";
  case ExecuteDecision::WAIT_POSE:
    return "WAIT_POSE";
  case ExecuteDecision::WAIT_MOTOR:
    return "WAIT_MOTOR";
  case ExecuteDecision::WAIT_FRONT_RANGE:
    return "WAIT_FRONT_RANGE";
  case ExecuteDecision::WAIT_TURN_CLEARANCE:
    return "WAIT_TURN_CLEARANCE";
  case ExecuteDecision::HEADING_RECOVERY:
    return "HEADING_RECOVERY";
  case ExecuteDecision::CLOSED_EDGE:
    return "CLOSED_EDGE";
  case ExecuteDecision::FRONT_BLOCKED:
    return "FRONT_BLOCKED";
  case ExecuteDecision::FAULT:
    return "FAULT";
  case ExecuteDecision::RELOCATE_TURN:
    return "RELOCATE_TURN";
  }
  return "UNKNOWN";
}
void PathExecutor::cancel() {
  path_ = {};
  phase_ = ExecuteStatus::IDLE;
  fault_ = ExecuteFault::NONE;
  index_ = leg_from_ = stable_ = 0;
  zero_since_ = progress_us_ = 0;
  best_distance_m_ = 0;
}
unsigned PathExecutor::next_landmark(unsigned from) const {
  unsigned target = from + 1;
  if (target >= path_.count)
    return from;
  const int dx = path_[target].x - path_[from].x,
            dy = path_[target].y - path_[from].y;
  for (; target + 1 < path_.count; ++target) {
    const auto cell = path_[target], next = path_[target + 1];
    if (next.x - cell.x != dx || next.y - cell.y != dy)
      break;
    const auto left = dx ? loc::NORTH : loc::WEST,
               right = dx ? loc::SOUTH : loc::EAST;
    // A known corridor has no lateral landmark. Treat it as one linear leg;
    // a mapped side opening/junction becomes the next observation target.
    if (map_.edge(cell, left) != loc::EdgeState::CLOSED ||
        map_.edge(cell, right) != loc::EdgeState::CLOSED)
      break;
  }
  return target;
}
bool PathExecutor::begin(const loc::MazeMap &m, loc::Cell goal,
                         const loc::PoseSnapshot &p, int64_t now,
                         uint32_t avoid_cells) {
  cancel();
  if (!m.valid() || !p.tracking || p.timestamp_us <= 0 ||
      p.timestamp_us > now || now - p.timestamp_us > 150000 ||
      !std::isfinite(p.x_m) || !std::isfinite(p.y_m) ||
      !std::isfinite(p.yaw_rad) || !std::isfinite(cfg_.max_speed_m_s) ||
      cfg_.max_speed_m_s <= 0 ||
      cfg_.max_speed_m_s >
          (motion_test::enabled ? motion_test::max_linear_m_s : .21f) ||
      !std::isfinite(cfg_.approach_gain_per_s) ||
      cfg_.approach_gain_per_s <= 0 || cfg_.approach_gain_per_s > 10 ||
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
    fault_ = ExecuteFault::INVALID_START;
    return false;
  }
  AStar::Generator planner;
  planner.setMaze(m);
  for (int i = 0; i < loc::kMazeN * loc::kMazeN; ++i) {
    const loc::Cell cell{i % loc::kMazeN, i / loc::kMazeN};
    if ((avoid_cells & (uint32_t(1) << i)) && cell != goal &&
        cell != (loc::Cell{p.grid_x, p.grid_y}))
      planner.addCollision(cell);
  }
  path_ = planner.findPath({p.grid_x, p.grid_y}, goal);
  if (path_.empty()) {
    phase_ = ExecuteStatus::NO_PATH;
    return false;
  }
  map_ = m;
  index_ = path_.count > 1 ? next_landmark(0) : 0;
  phase_ = path_.count > 1 ? ExecuteStatus::TURN : ExecuteStatus::DONE;
  if (path_.count > 1) {
    const auto from = path_[0], to = path_[index_];
    const float desired =
        std::atan2(float(to.y - from.y), float(to.x - from.x));
    // A small course error can be removed while advancing, using the same
    // 20-degree bound as ADVANCE heading recovery. Forcing a separate pivot
    // after end-tape replanning can strand a close-wall pose in a stationary
    // range-validity wait. Fresh pose/range, edge and front-stop gates still
    // apply before every translation command.
    if (std::fabs(loc::wrap_pi(desired - p.yaw_rad)) <= loc::deg2rad(20))
      phase_ = ExecuteStatus::ADVANCE;
  }
  leg_start_ = progress_us_ = now;
  best_distance_m_ = std::hypot((path_[index_].x + .5f) * loc::kCellM - p.x_m,
                                (path_[index_].y + .5f) * loc::kCellM - p.y_m);
  last_pose_ = last_motor_ = zero_since_ = 0;
  last_x_ = p.x_m;
  last_y_ = p.y_m;
  return true;
}
DriveRequest PathExecutor::update(const loc::PoseSnapshot &measured,
                                  const motor::MotorSnapshot &m,
                                  const BootstrapFrame &f, int64_t now,
                                  const HeadingObservation *heading,
                                  bool pivot_authorized) {
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
          !std::isfinite(f.range_m[i]) || f.range_m[i] <= loc::cfg::tof_min_m ||
          f.range_m[i] >= 1.5f)
        heading_turn = false;
  }
  if (heading_turn)
    p.yaw_rad = heading->yaw_rad;
  DriveRequest out{};
  out.status = phase_;
  out.waypoint = index_;
  out.fault = fault_;
  auto &diagnostic = out.diagnostics;
  diagnostic.waypoint = index_;
  if (index_ < path_.count)
    diagnostic.target = path_[index_];
  diagnostic.heading_only = heading_turn && !measured.tracking;
  if (phase_ == ExecuteStatus::IDLE || phase_ == ExecuteStatus::DONE ||
      phase_ == ExecuteStatus::NO_PATH || phase_ == ExecuteStatus::BLOCKED ||
      phase_ == ExecuteStatus::FAULT) {
    if (phase_ == ExecuteStatus::DONE)
      diagnostic.decision = ExecuteDecision::ARRIVED;
    return out;
  }
  auto fault = [&](ExecuteFault reason) {
    diagnostic.decision = ExecuteDecision::FAULT;
    phase_ = ExecuteStatus::FAULT;
    out.fault = fault_ = reason;
    out.status = phase_;
    return out;
  };
  if (now < leg_start_ || now < progress_us_ ||
      now - progress_us_ > cfg_.leg_timeout_us)
    return fault(ExecuteFault::LEG_TIMEOUT);
  if (m.status == motor::ControlStatus::DRIVER_FAULT ||
      m.status == motor::ControlStatus::TIMING_FAULT ||
      m.status == motor::ControlStatus::INVALID_COMMAND ||
      m.status == motor::ControlStatus::COMMAND_TIMEOUT)
    return fault(ExecuteFault::MOTOR_FAULT);
  const bool pose_invalid =
      (!heading_turn &&
       (!p.tracking || p.timestamp_us <= 0 || p.timestamp_us > now ||
        now - p.timestamp_us > 150000)) ||
      !std::isfinite(p.x_m) || !std::isfinite(p.y_m) ||
      !std::isfinite(p.yaw_rad);
  const bool motor_invalid =
      m.timestamp_us <= 0 || m.timestamp_us > now ||
      now - m.timestamp_us > 50000 || !std::isfinite(m.duty_left) ||
      !std::isfinite(m.duty_right) || std::fabs(m.duty_left) > 1 ||
      std::fabs(m.duty_right) > 1;
  if (pose_invalid || motor_invalid) {
    diagnostic.decision =
        pose_invalid ? ExecuteDecision::WAIT_POSE : ExecuteDecision::WAIT_MOTOR;
    stable_ = 0;
    zero_since_ = 0;
    out.status = ExecuteStatus::PAUSED;
    return out;
  }
  const auto target = path_[index_], previous = path_[index_ - 1];
  bool traversable = true;
  for (unsigned i = leg_from_ + 1; i <= index_; ++i)
    traversable = traversable && map_.can_step(path_[i - 1], path_[i]);
  if (!traversable) {
    diagnostic.decision = ExecuteDecision::CLOSED_EDGE;
    phase_ = ExecuteStatus::BLOCKED;
    out.status = phase_;
    return out;
  }
  const loc::Cell current{p.grid_x, p.grid_y};
  bool on_leg = false;
  for (unsigned i = leg_from_; i <= index_; ++i)
    on_leg = on_leg || current == path_[i];
  if (!on_leg)
    return fault(ExecuteFault::UNEXPECTED_CELL);
  const float dx = (target.x + 0.5f) * loc::kCellM - p.x_m,
              dy = (target.y + 0.5f) * loc::kCellM - p.y_m;
  const float distance = std::hypot(dx, dy);
  // Credit only monotonic measured distance reduction, never PWM, duplicate
  // frames, stale input or small range jitter. A stationary/stale leg still
  // faults; useful slow motion cannot renew the absolute run deadline.
  if (phase_ == ExecuteStatus::ADVANCE && p.tracking &&
      p.timestamp_us > last_pose_ && distance <= best_distance_m_ - .010f) {
    progress_us_ = now;
    best_distance_m_ = distance;
  }
  const float desired =
      std::atan2(float(target.y - previous.y), float(target.x - previous.x));
  const float error = loc::wrap_pi(desired - p.yaw_rad);
  const float forward = dx * std::cos(desired) + dy * std::sin(desired);
  const bool corner =
      index_ + 1 < path_.count &&
      (path_[index_ + 1].x - target.x != target.x - previous.x ||
       path_[index_ + 1].y - target.y != target.y - previous.y);
  const bool stop_required = corner || index_ + 1 == path_.count;
  const bool passed_center = forward <= 0;
  const float clearance = mapped_pivot_clearance(map_, p.x_m, p.y_m);
  const bool pivot_clear =
      motion_test::enabled || clearance >= kPivotRadiusM + kPivotMarginM;
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
  // PWM/gyro dwell and distinct pose confirmations prove separate things.
  // Range jitter restarts the pose confirmations, not the elapsed coast time.
  if (m.duty_left == 0 && m.duty_right == 0 && quiet) {
    if (!zero_since_)
      zero_since_ = now;
  } else
    zero_since_ = 0;
  const bool settled =
      zero_since_ > 0 && now - zero_since_ >= cfg_.coast_settle_us;
  diagnostic.geometry_valid = true;
  diagnostic.distance_m = distance;
  diagnostic.forward_m = forward;
  diagnostic.heading_error_rad = error;
  diagnostic.cross_track_m = -dx * std::sin(desired) + dy * std::cos(desired);
  diagnostic.course_error_rad = error;
  diagnostic.corner = corner;
  diagnostic.pivot_angle_rad = error;
  diagnostic.pivot_angle_valid = phase_ == ExecuteStatus::TURN;
  if (corner && phase_ == ExecuteStatus::ADVANCE) {
    const auto next = path_[index_ + 1];
    diagnostic.pivot_angle_rad = loc::wrap_pi(
        std::atan2(float(next.y - target.y), float(next.x - target.x)) -
        p.yaw_rad);
    diagnostic.pivot_angle_valid = true;
  }
  diagnostic.stop_required = stop_required;
  diagnostic.passed_center = passed_center;
  diagnostic.pivot_clearance_m = clearance;
  diagnostic.pivot_clear = pivot_clear;
  diagnostic.fresh_pose = fresh_pose;
  diagnostic.fresh_motor = fresh_motor;
  diagnostic.gyro_quiet = quiet;
  diagnostic.pose_stable = stable_pose;
  if (fresh_pose) {
    last_x_ = p.x_m;
    last_y_ = p.y_m;
  }
  last_pose_ = p.timestamp_us;
  last_motor_ = m.timestamp_us;
  if (phase_ == ExecuteStatus::TURN) {
    if (std::fabs(error) <= cfg_.heading_tolerance_rad) {
      if constexpr (motion_test::rolling_turns) {
        phase_ = ExecuteStatus::ADVANCE;
        if constexpr (motion_test::nonstop_only)
          return update(p, m, f, now, heading, pivot_authorized);
        out.linear_m_s = cfg_.max_speed_m_s;
        diagnostic.decision = ExecuteDecision::TURN_COMPLETE;
        out.status = phase_;
        return out;
      }
      diagnostic.decision = ExecuteDecision::WAIT_TURN_SETTLE;
      if (settled && fresh_pose && fresh_motor && stable_pose)
        ++stable_;
      else if (!settled || (fresh_pose && !stable_pose))
        stable_ = 0;
      if (stable_ >= 3) {
        diagnostic.decision = ExecuteDecision::TURN_COMPLETE;
        phase_ = ExecuteStatus::ADVANCE;
        stable_ = 0;
      }
    } else {
      if (!motion_test::enabled && !pivot_authorized &&
          clearance < kPivotRadiusM + kPivotHoldMarginM) {
        diagnostic.decision = ExecuteDecision::WAIT_TURN_CLEARANCE;
        stable_ = 0;
        zero_since_ = 0;
        out.status = ExecuteStatus::PAUSED;
        return out;
      }
      diagnostic.decision = ExecuteDecision::TURN_TO_HEADING;
      stable_ = 0;
      out.angular_rad_s =
          std::clamp(2 * error, -cfg_.max_turn_rad_s, cfg_.max_turn_rad_s);
      if constexpr (motion_test::rolling_turns)
        out.linear_m_s = motion_test::turn_feed_m_s;
    }
  } else if (phase_ == ExecuteStatus::ADVANCE) {
    const bool crossed_straight =
        !stop_required && forward <= 0 &&
        forward >= -cfg_.position_tolerance_m &&
        std::fabs(diagnostic.cross_track_m) <= cfg_.position_tolerance_m;
    if (distance <= cfg_.position_tolerance_m || crossed_straight) {
      if (!stop_required) {
        // A straight, known-open continuation needs no posture stop. Keep
        // front-range protection active on the waypoint handoff frame.
        if (f.tof_us[1] <= 0 || f.tof_us[1] > now ||
            now - f.tof_us[1] > 100000 || !std::isfinite(f.range_m[1]) ||
            f.range_m[1] <= 0 || f.range_m[1] >= 1.5f) {
          diagnostic.decision = ExecuteDecision::WAIT_FRONT_RANGE;
          out.status = ExecuteStatus::PAUSED;
          return out;
        }
        if (robot::geometry::front_edge_range(f.range_m[1]) <
            cfg_.obstacle_stop_m) {
          diagnostic.decision = ExecuteDecision::FRONT_BLOCKED;
          phase_ = ExecuteStatus::BLOCKED;
        } else {
          diagnostic.decision = ExecuteDecision::PASS_WAYPOINT;
          leg_from_ = index_;
          index_ = next_landmark(index_);
          leg_start_ = progress_us_ = now;
          best_distance_m_ = INFINITY;
          out.linear_m_s = cfg_.max_speed_m_s;
        }
      } else {
        if (motion_test::rolling_turns && corner) {
          diagnostic.decision = ExecuteDecision::NEXT_WAYPOINT;
          leg_from_ = index_;
          index_ = next_landmark(index_);
          phase_ = ExecuteStatus::TURN;
          leg_start_ = progress_us_ = now;
          best_distance_m_ = INFINITY;
          if constexpr (motion_test::nonstop_only)
            return update(p, m, f, now, heading, pivot_authorized);
          out.linear_m_s = cfg_.max_speed_m_s;
          out.status = phase_;
          out.waypoint = index_;
          return out;
        }
        diagnostic.decision = ExecuteDecision::ARRIVAL_STOP;
        phase_ = ExecuteStatus::SETTLE;
        stable_ = 0;
        zero_since_ = 0;
      }
    } else {
      if (std::fabs(error) > loc::deg2rad(20)) {
        diagnostic.decision = ExecuteDecision::HEADING_RECOVERY;
        phase_ = ExecuteStatus::TURN;
        stable_ = 0;
        if constexpr (motion_test::nonstop_only)
          return update(p, m, f, now, heading, pivot_authorized);
        if constexpr (motion_test::rolling_turns) {
          out.linear_m_s = motion_test::turn_feed_m_s;
          out.angular_rad_s =
              std::clamp(2 * error, -cfg_.max_turn_rad_s, cfg_.max_turn_rad_s);
        }
      } else {
        if (forward <= 0 && (!corner || forward < -cfg_.position_tolerance_m))
          return fault(ExecuteFault::TARGET_PASSED);
        if (f.tof_us[1] <= 0 || f.tof_us[1] > now ||
            now - f.tof_us[1] > 100000 || !std::isfinite(f.range_m[1]) ||
            f.range_m[1] <= 0 || f.range_m[1] >= 1.5f) {
          diagnostic.decision = ExecuteDecision::WAIT_FRONT_RANGE;
          stable_ = 0;
          zero_since_ = 0;
          out.status = ExecuteStatus::PAUSED;
          return out;
        }
        if (robot::geometry::front_edge_range(f.range_m[1]) <
            cfg_.obstacle_stop_m) {
          diagnostic.decision = ExecuteDecision::FRONT_BLOCKED;
          phase_ = ExecuteStatus::BLOCKED;
          out.status = phase_;
          return out;
        }
        if (corner && forward <= 0 && !pivot_clear) {
          diagnostic.decision = ExecuteDecision::WAIT_TURN_CLEARANCE;
          out.status = ExecuteStatus::PAUSED;
          return out;
        }
        out.linear_m_s =
            motion_test::enabled || !stop_required
                ? cfg_.max_speed_m_s
                : std::min(cfg_.max_speed_m_s,
                           std::max(.003f, cfg_.approach_gain_per_s * forward));
        diagnostic.decision = corner && distance <= cfg_.position_tolerance_m
                                  ? ExecuteDecision::APPROACH_CENTER
                                  : ExecuteDecision::ADVANCE;
        // Turning safely off-center can leave a small lateral offset on the
        // next leg. Steer from measured cross-track error while advancing so
        // that offset cannot accumulate and force another centering stop.
        // The lookahead bounds near-target yaw corrections in narrow cells.
        const float cross_track =
            -dx * std::sin(desired) + dy * std::cos(desired);
        float correction = std::clamp(
            std::atan2(cross_track, std::clamp(forward, .080f, .160f)),
            -loc::deg2rad(10), loc::deg2rad(10));
        // Leave enough travel time to remove the rear-axle turn's lateral
        // displacement before a side ray reaches its 15 mm invalid minimum.
        // This uses fresh measured ranges; it never makes an invalid pose
        // usable, and retains the front obstacle stop above.
        const bool side_fresh =
            f.tof_us[0] > 0 && f.tof_us[0] <= now &&
            now - f.tof_us[0] <= 100000 && f.tof_us[2] > 0 &&
            f.tof_us[2] <= now && now - f.tof_us[2] <= 100000 &&
            std::isfinite(f.range_m[0]) && std::isfinite(f.range_m[2]) &&
            f.range_m[0] > loc::cfg::tof_min_m &&
            f.range_m[2] > loc::cfg::tof_min_m &&
            f.range_m[0] < loc::cfg::tof_max_m &&
            f.range_m[2] < loc::cfg::tof_max_m;
        const bool close_side =
            side_fresh && std::min(f.range_m[0], f.range_m[2]) < .025f;
        if (close_side) {
          if (f.range_m[0] < f.range_m[2])
            correction = std::min(correction, -loc::deg2rad(5));
          else
            correction = std::max(correction, loc::deg2rad(5));
        }
        if (close_side)
          out.linear_m_s = std::min(out.linear_m_s, .060f);
        // Long arena rays can hit the adjacent wall when a course correction
        // tips them past a corner, leaving all three returns on one axis.
        // Prefer an observable course without reducing narrow-corridor
        // corrections that already see two independent wall normals.
        if (std::fabs(correction) > loc::deg2rad(5) && f.range_m[1] > .4f) {
          const float requested_correction = correction;
          for (float scale : {1.f, .8f, .6f, .4f, 0.f}) {
            const float course = desired + requested_correction * scale;
            unsigned axes = 0;
            for (const auto &mount : loc::cfg::lidar_mounts) {
              const auto hit =
                  loc::trace_maze_ray(map_, p.x_m, p.y_m, course, mount);
              if (hit.status == loc::RayStatus::HIT && !hit.ambiguous)
                axes |= 1U << hit.axis;
            }
            if (axes == 3) {
              correction = requested_correction * scale;
              break;
            }
          }
        }
        const float steering_error =
            loc::wrap_pi(desired + correction - p.yaw_rad);
        diagnostic.heading_error_rad = steering_error;
        out.angular_rad_s = std::clamp(2 * steering_error, -cfg_.max_turn_rad_s,
                                       cfg_.max_turn_rad_s);
      }
    }
  } else if (phase_ == ExecuteStatus::SETTLE) {
    diagnostic.decision = ExecuteDecision::WAIT_ARRIVAL_SETTLE;
    if (distance > 0.05f)
      return fault(ExecuteFault::SETTLE_DRIFT);
    // Arrival proves a stopped position, not permission for the next pivot.
    // Waiting here for circular turn clearance prevents the task-owned guard
    // from ever receiving that turn. Advance only after the normal fresh
    // quiet/stable dwell; TURN still requires its own clearance proof.
    if (distance > cfg_.position_tolerance_m + cfg_.settle_pose_step_m) {
      if (forward <= 0)
        return fault(ExecuteFault::SETTLE_DRIFT);
      diagnostic.decision = ExecuteDecision::REAPPROACH_CENTER;
      phase_ = ExecuteStatus::ADVANCE;
      stable_ = 0;
      zero_since_ = 0;
      out.status = phase_;
      return out;
    }
    if (settled && fresh_pose && fresh_motor && stable_pose)
      ++stable_;
    else if (!settled || (fresh_pose && !stable_pose))
      stable_ = 0;
    if (stable_ >= 3) {
      if (index_ + 1 >= path_.count) {
        diagnostic.decision = ExecuteDecision::ARRIVED;
        phase_ = ExecuteStatus::DONE;
      } else {
        diagnostic.decision = ExecuteDecision::NEXT_WAYPOINT;
        leg_from_ = index_;
        index_ = next_landmark(index_);
        phase_ = ExecuteStatus::TURN;
        leg_start_ = progress_us_ = now;
        best_distance_m_ = INFINITY;
      }
      stable_ = 0;
    }
  }
  out.status = phase_;
  out.waypoint = index_;
  diagnostic.zero_dwell_us = zero_since_ > 0 ? now - zero_since_ : 0;
  diagnostic.confirmations = stable_;
  return out;
}
} // namespace nav
