// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "AStar.hpp"
#include "motor_control/types.hpp"
#include "navigation/types.hpp"
namespace nav {
enum class ExecuteStatus : uint8_t {
  IDLE,
  TURN,
  ADVANCE,
  SETTLE,
  PAUSED,
  DONE,
  NO_PATH,
  BLOCKED,
  FAULT,
  FALLBACK,
  TIMEOUT,
  HALTED
};
enum class ExecuteFault : uint8_t {
  NONE,
  INVALID_START,
  LEG_TIMEOUT,
  MOTOR_FAULT,
  UNEXPECTED_CELL,
  TARGET_PASSED,
  SETTLE_DRIFT
};
const char *execute_fault_name(ExecuteFault fault);
enum class ExecuteDecision : uint8_t {
  INACTIVE,
  TURN_TO_HEADING,
  WAIT_TURN_SETTLE,
  ADVANCE,
  APPROACH_CENTER,
  ARRIVAL_STOP,
  WAIT_ARRIVAL_SETTLE,
  REAPPROACH_CENTER,
  TURN_COMPLETE,
  NEXT_WAYPOINT,
  PASS_WAYPOINT,
  ARRIVED,
  WAIT_POSE,
  WAIT_MOTOR,
  WAIT_FRONT_RANGE,
  WAIT_TURN_CLEARANCE,
  HEADING_RECOVERY,
  CLOSED_EDGE,
  FRONT_BLOCKED,
  FAULT,
  RELOCATE_TURN
};
const char *execute_decision_name(ExecuteDecision decision);
// Exact controller judgments for task-owned logging; no nominal motion is
// integrated here and no diagnostic field authorizes actuation.
struct ExecuteDiagnostics {
  ExecuteDecision decision = ExecuteDecision::INACTIVE;
  loc::Cell target{};
  float distance_m = 0, forward_m = 0, heading_error_rad = 0,
        pivot_angle_rad = 0, pivot_clearance_m = 0, cross_track_m = 0,
        course_error_rad = 0;
  int64_t zero_dwell_us = 0;
  unsigned confirmations = 0, waypoint = 0;
  bool geometry_valid = false, corner = false, passed_center = false,
       fresh_pose = false, fresh_motor = false, gyro_quiet = false,
       pose_stable = false, heading_only = false, pivot_clear = false,
       stop_required = false;
  bool pivot_angle_valid = false;
};
struct DriveRequest {
  float linear_m_s = 0, angular_rad_s = 0;
  ExecuteStatus status = ExecuteStatus::IDLE;
  unsigned waypoint = 0;
  ExecuteFault fault = ExecuteFault::NONE;
  ExecuteDiagnostics diagnostics{};
};
struct ExecutorConfig {
  float max_speed_m_s = 0.04f, max_turn_rad_s = 0.5f;
  float approach_gain_per_s = 1.f;
  float position_tolerance_m = 0.025f, heading_tolerance_rad = loc::deg2rad(5);
  float obstacle_stop_m = 0.025f, settle_gyro_rad_s = 0.05f,
        settle_pose_step_m = 0.005f;
  int64_t coast_settle_us =
      400000; // Zero PWM dwell, not measured wheel standstill.
  // Maximum time without 10 mm of new measured progress on a leg. The
  // task-owned absolute 86-second mission deadline is independent.
  int64_t leg_timeout_us = 30000000;
};
class PathExecutor {
public:
  /** @brief Initialize the portable cardinal waypoint executor.
   * @param[in] config Speed, arrival and timeout settings for the actual robot.
   * @returns No value.
   */
  explicit PathExecutor(const ExecutorConfig &config = ExecutorConfig{})
      : cfg_(config) {}
  /** @brief Plan a route from a trusted pose with the resolved map.
   * @param[in] map Current physical wall map.
   * @param[in] goal World cell in 0..4.
   * @param[in] pose Fresh tracked starting pose.
   * @param[in] now_us Shared monotonic clock.
   * @param[in] avoid_cells Temporary failed-turn cells excluded from this
   * route, except its source and final goal. The physical map is unchanged.
   * @returns true if a complete path exists, false otherwise; output remains
   * zero.
   */
  bool begin(const loc::MazeMap &map, loc::Cell goal,
             const loc::PoseSnapshot &pose, int64_t now_us,
             uint32_t avoid_cells = 0);
  /** @brief Compute one stop/turn/advance/settle request without touching
   * hardware.
   * @param[in] pose Latest localization snapshot.
   * @param[in] motor Latest applied-PWM request/status snapshot; no wheel
   * feedback.
   * @param[in] sensors Latest sensor frame for fresh front-range obstacle
   * checking.
   * @param[in] now_us Shared monotonic clock.
   * @param[in] heading Optional independent fresh heading for a
   * translation-unobservable pivot. Never authorizes forward motion or settled
   * arrival without a fresh pose.
   * @returns Nominal body velocity request; zero when stale, blocked, done or
   * faulted.
   * @note Pivot completion and stopped arrival require continuous zero PWM
   *       dwell, fresh quiet gyro and stable fresh poses. An already aligned
   *       route and collinear corridor cells advance without a settling stop.
   *       Turns require clearance for the complete swept
   *       chassis diagonal against mapped walls; exact center passage is not
   *       required. Unknown edges are treated as walls.
   *       This cannot detect wheel slip, stall or straight
   *       coasting that produces no yaw; calibrate dwell on the actual robot.
   */
  DriveRequest update(const loc::PoseSnapshot &pose,
                      const motor::MotorSnapshot &motor,
                      const BootstrapFrame &sensors, int64_t now_us,
                      const HeadingObservation *heading = nullptr,
                      // Task-owned guard has proved this pending arc; this
                      // never licenses translation or arrival on stale pose.
                      bool pivot_authorized = false);
  /** @brief Cancel a route and clear its pending commands.
   * @returns No value.
   */
  void cancel();
  /** @brief Inspect the current source-to-goal route.
   * @returns Fixed-capacity path, possibly empty.
   */
  const AStar::CoordinateList &path() const { return path_; }
  ExecuteStatus status() const { return phase_; }
  ExecuteFault fault() const { return fault_; }
  int64_t leg_started_us() const { return leg_start_; }

private:
  ExecutorConfig cfg_{};
  loc::MazeMap map_{};
  AStar::CoordinateList path_{};
  ExecuteStatus phase_ = ExecuteStatus::IDLE;
  ExecuteFault fault_ = ExecuteFault::NONE;
  unsigned index_ = 0, leg_from_ = 0, stable_ = 0;
  int64_t leg_start_ = 0, last_pose_ = 0, last_motor_ = 0, zero_since_ = 0;
  int64_t progress_us_ = 0;
  float best_distance_m_ = 0;
  float last_x_ = 0, last_y_ = 0;
  unsigned next_landmark(unsigned from) const;
};
} // namespace nav
