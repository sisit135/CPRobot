// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "navigation/turn_relocation_plan.hpp"

namespace nav {
enum class TurnRelocationPhase : uint8_t {
  IDLE,
  COAST,
  TRAVEL,
  SETTLE,
  COMPLETE,
  BLOCKED
};
// Pure snapshot controller. No motor/task calls. The caller must apply the
// existing absolute run deadline and cancel this object on terminal/end cues.
class TurnRelocation {
public:
  bool busy() const {
    return phase_ == TurnRelocationPhase::COAST ||
           phase_ == TurnRelocationPhase::TRAVEL ||
           phase_ == TurnRelocationPhase::SETTLE;
  }
  bool complete() const { return phase_ == TurnRelocationPhase::COMPLETE; }
  const TurnRelocationPlan &plan() const { return plan_; }
  uint32_t failed_cells() const { return failed_; }
  void cancel() { *this = TurnRelocation{}; }
  void finish() { phase_ = TurnRelocationPhase::IDLE; }
  bool begin(const loc::MazeMap &map, const loc::PoseSnapshot &p,
             loc::Cell goal, int64_t now) {
    if (busy() || attempts_ >= 3 || !fresh(p.timestamp_us, now, 150000))
      return false;
    const auto selected = plan_turn_relocation(map, p, goal, visited_, failed_);
    if (!selected.valid)
      return false;
    plan_ = selected;
    failed_ |= uint32_t(1) << (p.grid_y * 5 + p.grid_x);
    visited_ |= uint32_t(1) << (p.grid_y * 5 + p.grid_x);
    visited_ |= uint32_t(1) << (selected.cell.y * 5 + selected.cell.x);
    ++attempts_;
    started_ = now;
    quiet_since_ = last_pose_ = 0;
    stable_ = 0;
    phase_ = TurnRelocationPhase::COAST;
    return true;
  }
  DriveRequest update(const loc::MazeMap &map, const loc::PoseSnapshot &p,
                      const BootstrapFrame &f, const motor::MotorSnapshot &m,
                      int64_t now) {
    DriveRequest out{};
    out.status = ExecuteStatus::PAUSED;
    out.diagnostics.decision = ExecuteDecision::RELOCATE_TURN;
    out.diagnostics.target = plan_.cell;
    auto blocked = [&]() {
      phase_ = TurnRelocationPhase::BLOCKED;
      out.linear_m_s = out.angular_rad_s = 0;
      out.status = ExecuteStatus::BLOCKED;
      return out;
    };
    if (!busy())
      return phase_ == TurnRelocationPhase::BLOCKED ? blocked() : out;
    if (now < started_ || now - started_ > 20000000 || !map.valid())
      return blocked();
    if (!p.tracking || !p.grid_confident ||
        !fresh(p.timestamp_us, now, 150000) || !std::isfinite(p.x_m) ||
        !std::isfinite(p.y_m) || !std::isfinite(p.yaw_rad) ||
        !loc::valid_cell({p.grid_x, p.grid_y}) ||
        !fresh(m.timestamp_us, now, 50000) ||
        (m.status != motor::ControlStatus::IDLE &&
         m.status != motor::ControlStatus::RUNNING) ||
        !std::isfinite(m.duty_left) || !std::isfinite(m.duty_right) ||
        std::fabs(m.duty_left) > 1 || std::fabs(m.duty_right) > 1 ||
        !fresh(f.imu_us, now, 100000) || !std::isfinite(f.imu.gz_rps)) {
      quiet_since_ = 0;
      stable_ = 0;
      return out;
    }
    for (int i = 0; i < 3; ++i)
      if (!fresh(f.tof_us[i], now, 150000) || !std::isfinite(f.range_m[i]) ||
          f.range_m[i] <= loc::cfg::tof_min_m ||
          f.range_m[i] >= loc::cfg::tof_max_m) {
        quiet_since_ = 0;
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
        std::fabs(f.imu.gz_rps) < .05f) {
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
    if (phase_ == TurnRelocationPhase::COAST) {
      if (!settled)
        return out;
      if (!relocation_sweep_clear(map, p.x_m, p.y_m, p.yaw_rad, plan_.x_m,
                                  plan_.y_m, p.yaw_rad))
        return blocked();
      phase_ = TurnRelocationPhase::TRAVEL;
    }
    const float ux = std::cos(plan_.heading_rad),
                uy = std::sin(plan_.heading_rad);
    const float dx = plan_.x_m - p.x_m, dy = plan_.y_m - p.y_m;
    const float along = dx * ux + dy * uy;
    const float sign = plan_.reverse ? -1.f : 1.f;
    out.diagnostics.geometry_valid = true;
    out.diagnostics.distance_m = std::hypot(dx, dy);
    out.diagnostics.cross_track_m = -dx * uy + dy * ux;
    out.diagnostics.heading_error_rad =
        loc::wrap_pi(plan_.heading_rad - p.yaw_rad);
    if (std::fabs(out.diagnostics.heading_error_rad) > loc::deg2rad(10))
      return blocked();
    if (phase_ == TurnRelocationPhase::TRAVEL && sign * along <= .001f) {
      phase_ = TurnRelocationPhase::SETTLE;
      quiet_since_ = 0;
      stable_ = 0;
      return out;
    }
    if (phase_ == TurnRelocationPhase::SETTLE) {
      if (settled && p.grid_x == plan_.cell.x && p.grid_y == plan_.cell.y &&
          std::fabs(along) <= .010f &&
          mapped_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
                                  loc::wrap_pi(plan_.exit_yaw_rad - p.yaw_rad),
                                  .002f))
        phase_ = TurnRelocationPhase::COMPLETE;
      return out;
    }
    if (sign > 0 && robot::geometry::front_edge_range(f.range_m[1]) <= .025f)
      return out;
    out.linear_m_s = sign * std::clamp(2 * sign * along, .004f, .060f);
    out.angular_rad_s =
        std::clamp(2 * out.diagnostics.heading_error_rad, -.12f, .12f);
    const float look = sign * .008f;
    const float a = motor::cfg::wheel_mount_x_body_m;
    const float yaw =
        p.yaw_rad +
        (out.angular_rad_s == 0 ? 0 : std::copysign(.04f, out.angular_rad_s));
    if (!relocation_sweep_clear(map, p.x_m, p.y_m, p.yaw_rad,
                                p.x_m + look * std::cos(p.yaw_rad) +
                                    a * (std::cos(p.yaw_rad) - std::cos(yaw)),
                                p.y_m + look * std::sin(p.yaw_rad) +
                                    a * (std::sin(p.yaw_rad) - std::sin(yaw)),
                                yaw))
      return blocked();
    out.status = ExecuteStatus::ADVANCE;
    return out;
  }

private:
  TurnRelocationPlan plan_{};
  uint32_t visited_ = 0, failed_ = 0;
  unsigned attempts_ = 0, stable_ = 0;
  int64_t started_ = 0, quiet_since_ = 0, last_pose_ = 0;
  float last_x_ = 0, last_y_ = 0;
  TurnRelocationPhase phase_ = TurnRelocationPhase::IDLE;
  static bool fresh(int64_t stamp, int64_t now, int64_t age) {
    return stamp > 0 && stamp <= now && now - stamp <= age;
  }
};
} // namespace nav
