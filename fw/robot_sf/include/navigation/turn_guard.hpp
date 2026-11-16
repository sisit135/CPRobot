// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization/config.hpp"
#include "motor_control/config.hpp"
#include "navigation/path_executor.hpp"
#include "navigation/turn_clearance.hpp"
#include <algorithm>
#include <cmath>

#ifndef NAV_TURN_GUARD
#define NAV_TURN_GUARD 0
#endif

namespace nav {
enum class TurnGuardPhase : uint8_t {
  IDLE,
  COAST,
  LEAN,
  LEAN_STOP,
  SHIFT,
  SHIFT_STOP,
  ALIGN,
  ALIGN_STOP,
  RETURN,
  VERIFY,
  BLOCKED
};
inline const char *turn_guard_phase_name(TurnGuardPhase p) {
  switch (p) {
  case TurnGuardPhase::IDLE:
    return "IDLE";
  case TurnGuardPhase::COAST:
    return "COAST";
  case TurnGuardPhase::LEAN:
    return "LEAN";
  case TurnGuardPhase::LEAN_STOP:
    return "LEAN_STOP";
  case TurnGuardPhase::SHIFT:
    return "SHIFT";
  case TurnGuardPhase::SHIFT_STOP:
    return "SHIFT_STOP";
  case TurnGuardPhase::ALIGN:
    return "ALIGN";
  case TurnGuardPhase::ALIGN_STOP:
    return "ALIGN_STOP";
  case TurnGuardPhase::RETURN:
    return "RETURN";
  case TurnGuardPhase::VERIFY:
    return "VERIFY";
  case TurnGuardPhase::BLOCKED:
    return "BLOCKED";
  }
  return "UNKNOWN";
}

// Task-owned command interceptor. The caller holds the requested pivot while
// busy(); RHR still consumes gyro frames and checks its safety deadlines.
// Cancel this object on cancellation, terminal state or end cues.
// No nominal velocity is integrated into the measured pose.
class TurnGuard {
public:
  bool busy() const { return phase_ != TurnGuardPhase::IDLE; }
  bool pivot_authorized() const { return permitted_ && !busy(); }
  bool needs_relocation(int64_t now) const {
#if NAV_TURN_GUARD
    return phase_ == TurnGuardPhase::BLOCKED ||
           (phase_ == TurnGuardPhase::VERIFY && quiet_since_ > 0 &&
            now >= quiet_since_ && now - quiet_since_ >= 2000000 &&
            stable_ >= 3);
#else
    (void)now;
    return false;
#endif
  }
  TurnGuardPhase phase() const { return phase_; }
  float left_clearance_m() const { return left_; }
  float right_clearance_m() const { return right_; }
  void cancel() { *this = TurnGuard{}; }
  DriveRequest update(const loc::MazeMap &map,
                      const loc::PoseSnapshot &observed,
                      const BootstrapFrame &f, const motor::MotorSnapshot &m,
                      const DriveRequest &requested, int64_t now,
                      const HeadingObservation *heading = nullptr) {
#if !NAV_TURN_GUARD
    const auto &p = observed;
    (void)map;
    (void)p;
    (void)f;
    (void)m;
    (void)now;
    (void)heading;
    return requested;
#else
    auto p = observed;
    // Track continuous physical quiet independently of range/pose validity.
    // Range/pose validity gates motion separately; noisy ranges cannot erase
    // an uninterrupted fresh zero-PWM/gyro dwell. A missed update breaks it.
    const bool quiet_motor =
        fresh(m.timestamp_us, now, 50000) &&
        m.status == motor::ControlStatus::IDLE && m.duty_left == 0 &&
        m.duty_right == 0 && fresh(f.imu_us, now, 100000) &&
        std::isfinite(f.imu.gz_rps) && std::fabs(f.imu.gz_rps) <= .05f;
    if (!last_update_us_ || now < last_update_us_ ||
        now - last_update_us_ > 50000 || !quiet_motor)
      quiet_since_ = 0;
    last_update_us_ = now;
    if (quiet_motor && !quiet_since_)
      quiet_since_ = now;
    // Average measured axle position, not chassis position: rotation moves
    // the chassis around the axle. Never integrate commanded velocity.
    if (p.tracking && fresh(p.timestamp_us, now, 150000) &&
        std::isfinite(p.x_m) && std::isfinite(p.y_m) &&
        std::isfinite(p.yaw_rad)) {
      const float a = motor::cfg::wheel_mount_x_body_m;
      const float x = p.x_m + a * std::cos(p.yaw_rad);
      const float y = p.y_m + a * std::sin(p.yaw_rad);
      if (!filter_us_ || p.timestamp_us < filter_us_ ||
          p.timestamp_us - filter_us_ > 150000) {
        filtered_x_ = x;
        filtered_y_ = y;
        quiet_samples_ = 1;
      } else if (p.timestamp_us > filter_us_) {
        const float dt = (p.timestamp_us - filter_us_) * 1e-6f;
        const bool quiet = m.duty_left == 0 && m.duty_right == 0 &&
                           std::fabs(f.imu.gz_rps) < .05f;
        quiet_samples_ = quiet ? std::min(quiet_samples_ + 1, 32U) : 0;
        const float weight =
            quiet_samples_ > 0 ? 1.f / quiet_samples_ : dt / (.1f + dt);
        filtered_x_ += weight * (x - filtered_x_);
        filtered_y_ += weight * (y - filtered_y_);
      }
      filter_us_ = p.timestamp_us;
      p.x_m = filtered_x_ - a * std::cos(p.yaw_rad);
      p.y_m = filtered_y_ - a * std::sin(p.yaw_rad);
    }
    const bool pivot =
        (requested.linear_m_s == 0 && requested.angular_rad_s != 0) ||
        requested.diagnostics.decision == ExecuteDecision::WAIT_TURN_CLEARANCE;
    if (!busy() && !pivot) {
      if (requested.linear_m_s != 0 ||
          (requested.status != ExecuteStatus::TURN &&
           requested.status != ExecuteStatus::FALLBACK &&
           !(requested.status == ExecuteStatus::PAUSED &&
             requested.diagnostics.decision ==
                 ExecuteDecision::WAIT_TURN_CLEARANCE)))
        permitted_ = false;
      return requested;
    }
    const bool continuing = !busy() && permitted_;
    // A released pure pivot is geometrically anchored at its measured axle.
    // Wall fitting may be unavailable between cardinal headings. This local
    // envelope is not published as a translated localization measurement.
    if (continuing && heading && heading->valid &&
        fresh(heading->timestamp_us, now, 100000) &&
        std::isfinite(heading->yaw_rad)) {
      p.yaw_rad = heading->yaw_rad;
      p.x_m = pivot_axle_x_ -
              motor::cfg::wheel_mount_x_body_m * std::cos(p.yaw_rad);
      p.y_m = pivot_axle_y_ -
              motor::cfg::wheel_mount_x_body_m * std::sin(p.yaw_rad);
      p.timestamp_us = heading->timestamp_us;
      p.tracking = true;
    }
    DriveRequest out = requested;
    out.linear_m_s = out.angular_rad_s = 0;
    out.status = ExecuteStatus::PAUSED;
    if (!busy() && !continuing) {
      phase_ = TurnGuardPhase::COAST;
      started_ = now;
      quiet_since_ = last_pose_ = 0;
      stable_ = 0;
      pivot_target_valid_ = false;
    }
    // The 15 s deadline bounds active posture correction. An authorized
    // pivot has returned to IDLE and is owned by the executor/RHR deadlines;
    // continuing geometric checks must not resurrect an expired correction.
    if (phase_ == TurnGuardPhase::BLOCKED ||
        (busy() && (now < started_ || now - started_ > 15000000))) {
      phase_ = TurnGuardPhase::BLOCKED;
      out.status = ExecuteStatus::BLOCKED;
      return out;
    }
    if ((continuing && heading &&
         (!heading->valid || !fresh(heading->timestamp_us, now, 100000) ||
          !std::isfinite(heading->yaw_rad))) ||
        !fresh(p.timestamp_us, now, 150000) || !p.tracking || !map.valid() ||
        !std::isfinite(p.x_m) || !std::isfinite(p.y_m) ||
        !std::isfinite(p.yaw_rad) || !fresh(m.timestamp_us, now, 50000) ||
        !std::isfinite(m.duty_left) || !std::isfinite(m.duty_right) ||
        std::fabs(m.duty_left) > 1 || std::fabs(m.duty_right) > 1 ||
        (m.status != motor::ControlStatus::IDLE &&
         m.status != motor::ControlStatus::RUNNING) ||
        !fresh(f.imu_us, now, 100000) || !std::isfinite(f.imu.gz_rps)) {
      stable_ = 0;
      return out;
    }
    for (int i = 0; i < 3; ++i)
      if (!fresh(f.tof_us[i], now, 150000) || !std::isfinite(f.range_m[i]) ||
          f.range_m[i] <= loc::cfg::tof_min_m ||
          f.range_m[i] >= loc::cfg::tof_max_m) {
        stable_ = 0;
        return out;
      }
    const bool distinct = p.timestamp_us > last_pose_;
    const bool stable =
        last_pose_ && std::hypot(p.x_m - last_x_, p.y_m - last_y_) <= .005f;
    if (distinct) {
      last_pose_ = p.timestamp_us;
      last_x_ = p.x_m;
      last_y_ = p.y_m;
    }
    if (m.duty_left == 0 && m.duty_right == 0 &&
        std::fabs(f.imu.gz_rps) <= .05f) {
      if (!quiet_since_)
        quiet_since_ = now;
      if (distinct)
        stable_ = stable ? stable_ + 1 : 0;
    } else {
      quiet_since_ = 0;
      stable_ = 0;
    }
    const bool settled =
        quiet_since_ && now - quiet_since_ >= 400000 && stable_ >= 3;
    const float cardinal =
        std::round(p.yaw_rad / (loc::kPi / 2)) * (loc::kPi / 2);
    const float error = loc::wrap_pi(p.yaw_rad - cardinal);
    if (!pivot_target_valid_) {
      const float angle =
          requested.diagnostics.pivot_angle_valid
              ? requested.diagnostics.pivot_angle_rad
              : std::copysign(loc::kPi / 2, requested.angular_rad_s);
      pivot_positive_ = angle >= 0;
      pivot_yaw_ = p.yaw_rad + angle;
      pivot_target_valid_ = true;
    }
    left_ =
        cardinal_wall_clearance(f.range_m[0], loc::cfg::lidar_mounts[0], error);
    right_ =
        cardinal_wall_clearance(f.range_m[2], loc::cfg::lidar_mounts[2], error);
    if (continuing) {
      if (!mapped_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
                                   remaining_angle(p.yaw_rad))) {
        // Stop immediately, then re-observe at rest within the original
        // correction deadline. One noisy future-arc rejection is not a
        // permanent obstacle; no unproved angular command is forwarded.
        permitted_ = false;
        enter(TurnGuardPhase::COAST);
        return out;
      }
      out = authorized_request(requested, p.yaw_rad);
      // Hold the measured axle midpoint against unequal open-loop drives.
      if (out.angular_rad_s != 0 &&
          (!heading ||
           (observed.tracking && fresh(observed.timestamp_us, now, 150000)))) {
        const float dx = pivot_axle_x_ - filtered_x_;
        const float dy = pivot_axle_y_ - filtered_y_;
        out.linear_m_s = std::clamp(
            4 * (dx * std::cos(p.yaw_rad) + dy * std::sin(p.yaw_rad)), -.008f,
            .008f);
      }
      return out;
    }
    if (phase_ == TurnGuardPhase::COAST && settled) {
      if (mapped_observable_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
                                             remaining_angle(p.yaw_rad)) &&
          ranges_clear(p, f, cardinal)) {
        phase_ = TurnGuardPhase::IDLE;
        permitted_ = true;
        pivot_axle_x_ =
            p.x_m + motor::cfg::wheel_mount_x_body_m * std::cos(p.yaw_rad);
        pivot_axle_y_ =
            p.y_m + motor::cfg::wheel_mount_x_body_m * std::sin(p.yaw_rad);
        return authorized_request(requested, p.yaw_rad);
      }
      if (mapped_observable_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
                                             remaining_angle(p.yaw_rad))) {
        // Geometry is sufficient: wait for independent fresh range proof.
        return out;
      }
      if (std::fabs(error) > loc::deg2rad(15) || !plan(map, p, cardinal)) {
        phase_ = TurnGuardPhase::BLOCKED;
        out.status = ExecuteStatus::BLOCKED;
        return out;
      }
      enter(straight_ ? TurnGuardPhase::RETURN : TurnGuardPhase::LEAN);
    }
    if (phase_ == TurnGuardPhase::LEAN_STOP && settled)
      enter(TurnGuardPhase::SHIFT);
    if (phase_ == TurnGuardPhase::SHIFT_STOP && settled)
      enter(TurnGuardPhase::ALIGN);
    if (phase_ == TurnGuardPhase::ALIGN_STOP && settled) {
      // The safest turning posture may be ahead of or behind the original
      // center. Choose the final shunt from measured position, independently
      // of the drive direction used by SHIFT.
      const float remaining = (finish_x_ - p.x_m) * std::cos(base_) +
                              (finish_y_ - p.y_m) * std::sin(base_);
      direction_ = -std::copysign(1.f, remaining);
      enter(TurnGuardPhase::RETURN);
    }
    if (phase_ == TurnGuardPhase::VERIFY && settled) {
      if (straight_ && quiet_samples_ < 20)
        return out;
      if (ranges_clear(p, f, base_) &&
          mapped_observable_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
                                             remaining_angle(p.yaw_rad)) &&
          std::fabs(loc::wrap_pi(base_ - p.yaw_rad)) <= loc::deg2rad(2)) {
        phase_ = TurnGuardPhase::IDLE;
        permitted_ = true;
        pivot_axle_x_ =
            p.x_m + motor::cfg::wheel_mount_x_body_m * std::cos(p.yaw_rad);
        pivot_axle_y_ =
            p.y_m + motor::cfg::wheel_mount_x_body_m * std::sin(p.yaw_rad);
      }
      if (!permitted_ && straight_) {
        const float remaining = (finish_x_ - p.x_m) * std::cos(base_) +
                                (finish_y_ - p.y_m) * std::sin(base_);
        if (std::fabs(remaining) > .0003f) {
          direction_ = -std::copysign(1.f, remaining);
          enter(TurnGuardPhase::RETURN);
          return out;
        }
      }
      // Stay stopped for another fresh observation if verification has not
      // proved clearance yet. A single noisy return must not permanently
      // block an otherwise completed correction; the 15 s bound still applies.
      return out;
    }
    if (phase_ == TurnGuardPhase::LEAN || phase_ == TurnGuardPhase::ALIGN) {
      const float target = phase_ == TurnGuardPhase::LEAN ? lean_ : base_;
      const float e = loc::wrap_pi(target - p.yaw_rad);
      if (std::fabs(e) <= loc::deg2rad(.5f)) {
        enter(phase_ == TurnGuardPhase::LEAN ? TurnGuardPhase::LEAN_STOP
                                             : TurnGuardPhase::ALIGN_STOP);
      } else {
        out.angular_rad_s =
            std::copysign(std::clamp(3 * std::fabs(e), .08f, .35f), e);
        out.status = ExecuteStatus::TURN;
      }
    } else if (phase_ == TurnGuardPhase::SHIFT ||
               phase_ == TurnGuardPhase::RETURN) {
      const bool shift = phase_ == TurnGuardPhase::SHIFT;
      const float target_x = shift ? shift_x_ : finish_x_;
      const float target_y = shift ? shift_y_ : finish_y_;
      const float yaw = shift ? lean_ : base_;
      const float direction = shift ? direction_ : -direction_;
      // SHIFT must reach the lateral target precisely; a heading-only
      // projection leaves an uncorrectable lateral error for RETURN.
      const float remaining =
          shift ? direction *
                      ((target_x - p.x_m) * -std::sin(base_) +
                       (target_y - p.y_m) * std::cos(base_)) /
                      std::sin(lean_ - base_)
                : direction * ((target_x - p.x_m) * std::cos(yaw) +
                               (target_y - p.y_m) * std::sin(yaw));
      if (remaining <= ((shift || straight_) ? .0002f : .001f)) {
        enter(shift ? TurnGuardPhase::SHIFT_STOP : TurnGuardPhase::VERIFY);
      } else {
        out.linear_m_s = direction * std::min(straight_ ? .006f : .045f,
                                              std::max(.004f, 2 * remaining));
        out.angular_rad_s =
            std::clamp(3 * loc::wrap_pi(yaw - p.yaw_rad), -.25f, .25f);
        out.status = ExecuteStatus::ADVANCE;
      }
    }
    // Include a bounded coast envelope before every nonzero recovery request.
    const float look =
        out.linear_m_s == 0 ? 0 : std::copysign(.006f, out.linear_m_s);
    const float look_yaw =
        p.yaw_rad +
        std::copysign(out.angular_rad_s == 0 ? 0 : .04f, out.angular_rad_s);
    const float axle = motor::cfg::wheel_mount_x_body_m;
    if ((out.linear_m_s != 0 || out.angular_rad_s != 0) &&
        !sweep(map, p.x_m, p.y_m, p.yaw_rad,
               p.x_m + look * std::cos(p.yaw_rad) +
                   axle * (std::cos(p.yaw_rad) - std::cos(look_yaw)),
               p.y_m + look * std::sin(p.yaw_rad) +
                   axle * (std::sin(p.yaw_rad) - std::sin(look_yaw)),
               look_yaw)) {
      phase_ = TurnGuardPhase::BLOCKED;
      out.linear_m_s = out.angular_rad_s = 0;
      out.status = ExecuteStatus::BLOCKED;
    }
    return out;
#endif
  }

private:
  TurnGuardPhase phase_ = TurnGuardPhase::IDLE;
  bool permitted_ = false;
  float left_ = 0, right_ = 0;
#if NAV_TURN_GUARD
  unsigned quiet_samples_ = 0;
  int64_t filter_us_ = 0, last_update_us_ = 0;
  float filtered_x_ = 0, filtered_y_ = 0;
  bool straight_ = false;
  bool pivot_target_valid_ = false, pivot_positive_ = true;
  float pivot_yaw_ = 0;
  float pivot_axle_x_ = 0, pivot_axle_y_ = 0;
  int64_t started_ = 0, quiet_since_ = 0, last_pose_ = 0;
  unsigned stable_ = 0;
  float last_x_ = 0, last_y_ = 0, base_ = 0, lean_ = 0, direction_ = 0;
  float shift_x_ = 0, shift_y_ = 0, finish_x_ = 0, finish_y_ = 0;
  float remaining_angle(float yaw) const {
    float angle = loc::wrap_pi(pivot_yaw_ - yaw);
    // Preserve the requested side of a half-turn across +/-pi. Tiny heading
    // changes must not swap the proved sweep while RHR keeps turning the
    // original way. A small overshoot may still be corrected in reverse.
    if (std::fabs(angle) > loc::kPi / 2 && (angle > 0) != pivot_positive_)
      angle += pivot_positive_ ? 2 * loc::kPi : -2 * loc::kPi;
    return angle;
  }
  DriveRequest authorized_request(DriveRequest requested, float yaw) const {
    const float remaining = remaining_angle(yaw);
    // Near +/-pi, executor heading noise can reverse the shortest-angle sign.
    // Apply the direction of the arc actually proved by this guard. Small
    // overshoot corrections remain free to reverse after the halfway point.
    if (requested.angular_rad_s != 0 && std::fabs(remaining) > loc::kPi / 2)
      requested.angular_rad_s =
          std::copysign(std::fabs(requested.angular_rad_s), remaining);
    return requested;
  }
  static bool fresh(int64_t stamp, int64_t now, int64_t age) {
    return stamp > 0 && stamp <= now && now - stamp <= age;
  }
  void enter(TurnGuardPhase p) {
    phase_ = p;
    quiet_since_ = 0;
    stable_ = 0;
  }
  static bool sweep(const loc::MazeMap &map, float x, float y, float yaw,
                    float tx, float ty, float target_yaw) {
    const float angle = loc::wrap_pi(target_yaw - yaw);
    const int steps = 1 + int(std::max(std::hypot(tx - x, ty - y) / .001f,
                                       std::fabs(angle) / loc::deg2rad(1)));
    for (int i = 0; i <= steps; ++i) {
      const float t = float(i) / steps;
      if (!mapped_footprint_clear(map, x + t * (tx - x), y + t * (ty - y),
                                  yaw + t * angle))
        return false;
    }
    return true;
  }
  bool ranges_clear(const loc::PoseSnapshot &p, const BootstrapFrame &f,
                    float cardinal) const {
    float required[3]{};
    const float angle = remaining_angle(p.yaw_rad);
    const int steps = 1 + int(std::fabs(angle) / loc::deg2rad(1));
    const float axle = motor::cfg::wheel_mount_x_body_m;
    for (int i = 0; i <= steps; ++i) {
      const float a = p.yaw_rad + angle * float(i) / steps;
      const float dx = axle * (std::cos(p.yaw_rad) - std::cos(a));
      const float dy = axle * (std::sin(p.yaw_rad) - std::sin(a));
      const float c = std::cos(a - cardinal), s = std::sin(a - cardinal);
      const float forward = std::cos(cardinal) * dx + std::sin(cardinal) * dy;
      const float lateral = -std::sin(cardinal) * dx + std::cos(cardinal) * dy;
      const float forward_extent = robot::geometry::footprint_support(c, -s);
      const float left_extent = robot::geometry::footprint_support(s, c);
      const float right_extent = robot::geometry::footprint_support(-s, -c);
      required[0] = std::max(required[0], lateral + left_extent + .002f);
      required[1] = std::max(required[1], forward + forward_extent + .002f);
      required[2] = std::max(required[2], -lateral + right_extent + .002f);
    }
    const float error = loc::wrap_pi(p.yaw_rad - cardinal);
    for (int i = 0; i < 3; ++i)
      if (cardinal_wall_clearance(f.range_m[i], loc::cfg::lidar_mounts[i],
                                  error) < required[i])
        return false;
    return true;
  }
  bool plan(const loc::MazeMap &map, const loc::PoseSnapshot &p,
            float cardinal) {
    if (!loc::valid_cell({p.grid_x, p.grid_y}))
      return false;
    base_ = cardinal;
    const float lx = -std::sin(base_), ly = std::cos(base_);
    const float cx = (p.grid_x + .5f) * loc::kCellM;
    const float cy = (p.grid_y + .5f) * loc::kCellM;
    const float center_offset = (cx - p.x_m) * lx + (cy - p.y_m) * ly;
    // First try a short measured translation along the current heading.
    // A rear axle can require longitudinal bias even when lateral centering
    // is already correct. Prove both the approach and the final pivot.
    straight_ = false;
    float first = 0, last = 0;
    bool straight_found = false;
    // A feasible interval can be narrower than the coarse 0.25 mm grid
    // (GDB seed 4159659801). Only when that grid found nothing, test its
    // midpoints. Every selected point and the interval midpoint still pass
    // the complete approach and rear-axle arc proofs with unchanged padding.
    for (int refinement = 0; refinement < 2 && !straight_found; ++refinement) {
      for (int step = -96; step <= 96 - refinement; ++step) {
        const float distance = (step + .5f * refinement) * .00025f;
        const float x = p.x_m + distance * std::cos(p.yaw_rad);
        const float y = p.y_m + distance * std::sin(p.yaw_rad);
        if (mapped_observable_axle_pivot_clear(
                map, x, y, p.yaw_rad, remaining_angle(p.yaw_rad), .0024f) &&
            sweep(map, p.x_m, p.y_m, p.yaw_rad, x, y, p.yaw_rad)) {
          if (!straight_found)
            first = distance;
          last = distance;
          straight_found = true;
        }
      }
    }

    if (straight_found) {
      const float distance = (first + last) / 2;
      const float x = p.x_m + distance * std::cos(p.yaw_rad);
      const float y = p.y_m + distance * std::sin(p.yaw_rad);
      // Recheck the midpoint: disconnected feasible intervals cannot license
      // an unproved pose between their endpoints.
      if (mapped_observable_axle_pivot_clear(
              map, x, y, p.yaw_rad, remaining_angle(p.yaw_rad), .0024f) &&
          sweep(map, p.x_m, p.y_m, p.yaw_rad, x, y, p.yaw_rad)) {
        straight_ = true;
        base_ = p.yaw_rad;
        finish_x_ = x;
        finish_y_ = y;
        direction_ = -std::copysign(1.f, distance);
        return true;
      }
    }
    // A lateral bias can also require a fore/aft shunt: returning to the
    // original longitudinal coordinate is not always the safest place to
    // pivot. Search bounded final postures and prove every leg, including a
    // reverse leg against mapped rear walls. Unknown edges remain closed.
    for (float longitudinal : {0.f, -.012f, .012f, -.024f, .024f}) {
      float first_offset = 0, last_offset = 0;
      bool found = false;
      for (int step = -96; step <= 96; ++step) {
        const float offset = center_offset + step * .00025f;
        if (std::hypot(offset, longitudinal) > .040f)
          continue;
        const float x = p.x_m + lx * offset + std::cos(base_) * longitudinal;
        const float y = p.y_m + ly * offset + std::sin(base_) * longitudinal;
        if (mapped_observable_axle_pivot_clear(
                map, x, y, base_, remaining_angle(base_), .0024f)) {
          if (!found)
            first_offset = offset;
          last_offset = offset;
          found = true;
        }
      }
      if (!found)
        continue;
      // Favor the interior of the safe interval, retaining endpoints as
      // alternatives when an intervening wall makes that interval disjoint.
      for (float offset :
           {(first_offset + last_offset) / 2, first_offset, last_offset}) {
        if (std::fabs(offset) < .001f)
          continue;
        finish_x_ = p.x_m + lx * offset + std::cos(base_) * longitudinal;
        finish_y_ = p.y_m + ly * offset + std::sin(base_) * longitudinal;
        if (!mapped_observable_axle_pivot_clear(map, finish_x_, finish_y_,
                                                base_, remaining_angle(base_),
                                                .0024f))
          continue;
        for (float degrees : {16.f, 14.f, 12.f})
          for (float direction : {1.f, -1.f}) {
            direction_ = direction;
            lean_ = base_ +
                    direction * std::copysign(loc::deg2rad(degrees), offset);
            const float axle = motor::cfg::wheel_mount_x_body_m;
            const float rotation_lateral =
                axle * ((std::cos(p.yaw_rad) - std::cos(base_)) * lx +
                        (std::sin(p.yaw_rad) - std::sin(base_)) * ly);
            const float length = (offset - rotation_lateral) /
                                 (direction * std::sin(lean_ - base_));
            const float lean_x =
                p.x_m + axle * (std::cos(p.yaw_rad) - std::cos(lean_));
            const float lean_y =
                p.y_m + axle * (std::sin(p.yaw_rad) - std::sin(lean_));
            shift_x_ = lean_x + direction * length * std::cos(lean_);
            shift_y_ = lean_y + direction * length * std::sin(lean_);
            const float aligned_x =
                shift_x_ + axle * (std::cos(lean_) - std::cos(base_));
            const float aligned_y =
                shift_y_ + axle * (std::sin(lean_) - std::sin(base_));
            const float lean_angle = loc::wrap_pi(lean_ - p.yaw_rad);
            if (mapped_observable_axle_pivot_clear(
                    map, p.x_m, p.y_m, p.yaw_rad,
                    lean_angle + std::copysign(.04f, lean_angle)) &&
                sweep(map, lean_x, lean_y, lean_, shift_x_, shift_y_, lean_) &&
                mapped_observable_axle_pivot_clear(
                    map, shift_x_, shift_y_, lean_,
                    loc::wrap_pi(base_ - lean_)) &&
                sweep(map, aligned_x, aligned_y, base_, finish_x_, finish_y_,
                      base_))
              return true;
          }
      }
    }
    return false;
  }
#endif
};
} // namespace nav
