// SPDX-License-Identifier: GPL-3.0-only

#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/end_cell.hpp"
#include "navigation/feature_tasks.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/navigation.hpp"
#include "navigation/path_executor.hpp"
#include "navigation/right_hand_rule.hpp"
#include "navigation/startup_turn.hpp"
#include "navigation/turn_guard.hpp"
#include "navigation/turn_relocation.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
namespace nav {
#ifdef ROBOT_LIFECYCLE_TEST
// Supplied only by the native interactive lifecycle executable.
bool lifecycle_force_rhr();
bool lifecycle_startup_only();
#endif
namespace {
static const char *TAG = "navigation";
std::atomic<bool> halted{false};
struct GoalCommand {
  loc::Cell goal;
  bool cancel = false;
};
QueueHandle_t commands = nullptr, states = nullptr;
ResolverConfig config;
NavigationEngine selected = NavigationEngine::FEATURE;
RightHandConfig fallback_config;
const char *resolve_status_name(ResolveStatus status) {
  switch (status) {
  case ResolveStatus::WAIT_CALIBRATION:
    return "WAIT_CALIBRATION";
  case ResolveStatus::WAIT_SENSORS:
    return "WAIT_SENSORS";
  case ResolveStatus::NO_CANDIDATE:
    return "NO_CANDIDATE";
  case ResolveStatus::AMBIGUOUS:
    return "AMBIGUOUS";
  case ResolveStatus::CONFIRMING:
    return "CONFIRMING";
  case ResolveStatus::RESOLVED:
    return "RESOLVED";
  case ResolveStatus::INVALID_CONFIG:
    return "INVALID_CONFIG";
  }
  return "UNKNOWN";
}
const char *fallback_phase_name(FallbackPhase phase) {
  switch (phase) {
  case FallbackPhase::IDLE:
    return "IDLE";
  case FallbackPhase::DECIDE:
    return "DECIDE";
  case FallbackPhase::TURN:
    return "TURN";
  case FallbackPhase::COAST:
    return "COAST";
  case FallbackPhase::CLEAR:
    return "CLEAR";
  case FallbackPhase::FOLLOW:
    return "FOLLOW";
  case FallbackPhase::PAUSED:
    return "PAUSED";
  case FallbackPhase::FAULT:
    return "FAULT";
  case FallbackPhase::CENTER:
    return "CENTER";
  }
  return "UNKNOWN";
}
const char *execute_status_name(ExecuteStatus status) {
  switch (status) {
  case ExecuteStatus::IDLE:
    return "IDLE";
  case ExecuteStatus::TURN:
    return "TURN";
  case ExecuteStatus::ADVANCE:
    return "ADVANCE";
  case ExecuteStatus::SETTLE:
    return "SETTLE";
  case ExecuteStatus::PAUSED:
    return "PAUSED";
  case ExecuteStatus::DONE:
    return "DONE";
  case ExecuteStatus::NO_PATH:
    return "NO_PATH";
  case ExecuteStatus::BLOCKED:
    return "BLOCKED";
  case ExecuteStatus::FAULT:
    return "FAULT";
  case ExecuteStatus::FALLBACK:
    return "FALLBACK";
  case ExecuteStatus::TIMEOUT:
    return "TIMEOUT";
  case ExecuteStatus::HALTED:
    return "HALTED";
  }
  return "UNKNOWN";
}
void log_route(const PathExecutor &executor, const loc::PoseSnapshot &pose,
               loc::Cell goal, bool accepted) {
  robot_log::write(accepted ? robot_log::Level::INFO : robot_log::Level::WARN,
                   TAG, "plan (%d,%d)->(%d,%d) %s cells=%u reason=%s",
                   pose.grid_x, pose.grid_y, goal.x, goal.y,
                   execute_status_name(executor.status()),
                   executor.path().count, execute_fault_name(executor.fault()));
  const auto &route = executor.path();
  for (unsigned first = 0; first < route.count; first += 8) {
    char cells[72]{};
    unsigned used = 0;
    for (unsigned i = first; i < std::min(first + 8, route.count); ++i)
      used += unsigned(std::snprintf(cells + used, sizeof(cells) - used,
                                     " (%d,%d)", route[i].x, route[i].y));
    robot_log::write(robot_log::Level::INFO, TAG, "route[%u]:%s", first, cells);
  }
}
struct PathLogState {
  ExecuteStatus status = ExecuteStatus::IDLE;
  ExecuteDecision decision = ExecuteDecision::INACTIVE;
  unsigned waypoint = 0;
  int64_t last_report_us = 0, zero_since_us = 0, last_veer_warn_us = 0;
  bool reported = false, zero_warned = false, end_wait = false,
       veer_warned = false;
  void update(const DriveRequest &drive, const loc::PoseSnapshot &pose,
              const motor::MotorSnapshot &motor, const FeatureSnapshot &feature,
              bool active, bool stop_limited, int64_t now) {
    const auto &d = drive.diagnostics;
    // The inactive publication repeats the fault status with no new executor
    // sample. Retain the actual failing waypoint/judgment for terminal logs.
    if (!active && reported && d.decision == ExecuteDecision::INACTIVE &&
        status == drive.status)
      return;
    if (active && drive.linear_m_s == 0 && drive.angular_rad_s == 0) {
      if (!zero_since_us)
        zero_since_us = now;
      if (!zero_warned && now - zero_since_us >= 5000000) {
        robot_log::write(robot_log::Level::WARN, TAG,
                         "zero request >5s wp=%u phase=%s why=%s",
                         drive.waypoint, execute_status_name(drive.status),
                         execute_decision_name(d.decision));
        zero_warned = true;
      }
    } else {
      zero_since_us = 0;
      zero_warned = false;
    }
    const bool change = !reported || status != drive.status ||
                        decision != d.decision || waypoint != drive.waypoint;
    if (active && change &&
        (d.decision == ExecuteDecision::WAIT_TURN_CLEARANCE ||
         d.decision == ExecuteDecision::REAPPROACH_CENTER ||
         d.decision == ExecuteDecision::HEADING_RECOVERY ||
         d.decision == ExecuteDecision::FRONT_BLOCKED))
      robot_log::write(robot_log::Level::WARN, TAG,
                       "unsafe wp=%u why=%s pivot=%.3f cross=%.3f yaw=%.1f",
                       drive.waypoint, execute_decision_name(d.decision),
                       double(d.pivot_clearance_m), double(d.cross_track_m),
                       double(d.course_error_rad * 180 / loc::kPi));
    const bool advancing = active && d.geometry_valid && drive.linear_m_s > 0 &&
                           (d.decision == ExecuteDecision::ADVANCE ||
                            d.decision == ExecuteDecision::APPROACH_CENTER ||
                            d.decision == ExecuteDecision::PASS_WAYPOINT);
    if (!active || waypoint != drive.waypoint ||
        (advancing && std::fabs(d.cross_track_m) < .003f &&
         std::fabs(d.course_error_rad) < loc::deg2rad(7)))
      veer_warned = false;
    if (advancing && !veer_warned &&
        (std::fabs(d.cross_track_m) > .005f ||
         std::fabs(d.course_error_rad) > loc::deg2rad(10)) &&
        (!last_veer_warn_us || now - last_veer_warn_us >= 1000000)) {
      robot_log::write(
          robot_log::Level::WARN, TAG,
          "veering off path wp=%u cross=%.3f course=%.1f v=%.3f w=%.3f",
          drive.waypoint, double(d.cross_track_m),
          double(d.course_error_rad * 180 / loc::kPi), double(drive.linear_m_s),
          double(drive.angular_rad_s));
      veer_warned = true;
      last_veer_warn_us = now;
    }
    if (!change && (!active || now - last_report_us < 1000000))
      return;
    robot_log::write(
        robot_log::Level::INFO, TAG, "wp=%u phase=%s why=%s v=%.3f w=%.3f",
        d.geometry_valid ? d.waypoint : drive.waypoint,
        execute_status_name(drive.status), execute_decision_name(d.decision),
        double(drive.linear_m_s), double(drive.angular_rad_s));
    if (d.geometry_valid)
      robot_log::write(
          robot_log::Level::INFO, TAG,
          "target=(%d,%d) dist=%.3f fwd=%.3f yaw=%.1f pivot=%.3f safe=%u",
          d.target.x, d.target.y, double(d.distance_m), double(d.forward_m),
          double(d.heading_error_rad * 180 / loc::kPi),
          double(d.pivot_clearance_m), unsigned(d.pivot_clear));
    if (drive.status == ExecuteStatus::SETTLE ||
        d.decision == ExecuteDecision::WAIT_TURN_SETTLE)
      robot_log::write(
          robot_log::Level::INFO, TAG,
          "settle zero_ms=%lld quiet=%u stable=%u fresh=%u/%u confirms=%u",
          static_cast<long long>(d.zero_dwell_us / 1000),
          unsigned(d.gyro_quiet), unsigned(d.pose_stable),
          unsigned(d.fresh_pose), unsigned(d.fresh_motor), d.confirmations);
    if (drive.status == ExecuteStatus::PAUSED)
      robot_log::write(
          robot_log::Level::INFO, TAG,
          "wait tracking=%u pose_ms=%lld motor_ms=%lld heading_only=%u",
          unsigned(pose.tracking),
          static_cast<long long>((now - pose.timestamp_us) / 1000),
          static_cast<long long>((now - motor.timestamp_us) / 1000),
          unsigned(d.heading_only));
    if (feature.stop.valid && drive.linear_m_s > 0)
      robot_log::write(
          robot_log::Level::INFO, TAG,
          "stop target=(%d,%d) dist=%.3f coast=%.3f speed=%.3f cap=%u stop=%u",
          feature.stop.target.x, feature.stop.target.y,
          double(feature.stop.distance_m), double(feature.stop.allowance_m),
          double(feature.stop.speed_m_s), unsigned(stop_limited),
          unsigned(feature.stop.should_stop));
    status = drive.status;
    decision = d.decision;
    waypoint = drive.waypoint;
    last_report_us = now;
    reported = true;
  }
};
void path_task(void *) {
  if constexpr (motion_test::enabled)
    robot_log::write(
        robot_log::Level::INFO, TAG,
        "motion=rolling_max turn_guard=0 full_pwm=1 rolling_turns=1");
  else if constexpr (motion_test::nonstop_only)
    robot_log::write(
        robot_log::Level::INFO, TAG,
        "motion=nonstop_turn turn_guard=%u full_pwm=0 rolling_turns=1",
        unsigned(NAV_TURN_GUARD));
  ExecutorConfig execute_config;
  execute_config.max_speed_m_s = .21f;
  execute_config.max_turn_rad_s = 1.8f;
  execute_config.approach_gain_per_s = 4.f;
  execute_config.position_tolerance_m = .010f;
  if constexpr (motion_test::enabled)
    execute_config.max_speed_m_s = motion_test::max_linear_m_s();
  PathExecutor executor(execute_config);
#if NAV_TURN_GUARD
  TurnGuard turn_guard;
  TurnRelocation turn_relocation;
  DriveRequest guarded_request{};
  TurnGuardPhase reported_guard = TurnGuardPhase::IDLE;
  bool rhr_startup_turn_authorized = false;
#endif
  RightHandRule fallback(fallback_config);
  EndCellGate end;
  NavigationSnapshot state;
  state.engine = selected;
  loc::MazeMap map;
  int64_t stage_us = 0, goal_us = 0;
  loc::Cell goal{};
  bool active = false, using_fallback = false, fallback_latched = false,
       ending = false;
  FallbackPhase reported_phase = FallbackPhase::IDLE;
  ExecuteStatus inactive_status = ExecuteStatus::IDLE;
  ExecuteFault inactive_fault = ExecuteFault::NONE;
  bool failure_reported = false;
  PathLogState path_log;
  bool startup_scan = false, startup_scan_attempted = false,
       startup_motor_started = false, startup_motor_waiting = false;
  unsigned startup_scan_confirmations = 0;
  int64_t startup_scan_frame_us = 0;
  std::array<uint32_t, 4> startup_scan_cells{};
  auto bind_map = [&](const Resolution &seed) {
    map = config.variants[static_cast<unsigned>(seed.kind)];
    state.kind = seed.kind;
    robot_log::write(robot_log::Level::INFO, TAG,
                     "map variant latched kind=%u cell=(%d,%d) "
                     "yaw=%.3f score=%.2f margin=%.2f",
                     unsigned(seed.kind), seed.cell.x, seed.cell.y,
                     double(seed.yaw_rad), double(seed.score),
                     double(seed.margin));
    hal_navigation_resolved(map);
    state.ekf_started = loc::localization_start(map, seed);
  };
#ifdef ROBOT_LIFECYCLE_TEST
  const bool forced_rhr = lifecycle_force_rhr();
  if (forced_rhr) {
    // Exercise the real RHR controller even when wall features are ambiguous.
    // Motor readiness and the normal goal/run/sensor guards still apply.
    fallback_latched = true;
    state.fallback_reason = FallbackReason::FORCED_TEST;
    stage_us = hal_time_us();
    state.startup = motor::motor_control_start() ? StartupStatus::STARTING_MOTOR
                                                 : StartupStatus::FAULT;
    robot_log::write(robot_log::Level::WARN, TAG,
                     "forced RHR startup: awaiting motor READY");
  }
#endif
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    const int64_t now = hal_time_us();
    state.run = motor::motor_run_state();
    if (halted.load(std::memory_order_acquire) || state.run.terminal()) {
#if NAV_TURN_GUARD
      turn_guard.cancel();
      turn_relocation.cancel();
#endif
      executor.cancel();
      fallback.cancel();
      active = false;
      state.fallback = false;
      state.fallback_phase = reported_phase = FallbackPhase::IDLE;
      feature_task_halt();
      loc::localization_halt();
      state.startup =
          state.run.state == RunState::TIMEOUT
              ? StartupStatus::TIMEOUT
              : (state.run.state == RunState::GOAL ? StartupStatus::DONE
                                                   : StartupStatus::HALTED);
      state.execution = static_cast<uint8_t>(
          state.run.state == RunState::TIMEOUT
              ? ExecuteStatus::TIMEOUT
              : (state.run.state == RunState::GOAL ? ExecuteStatus::DONE
                                                   : ExecuteStatus::HALTED));
      GoalCommand discard{};
      (void)xQueueReceive(commands, &discard, 0);
      if (!state.terminal_reported) {
        robot_log::write(state.run.state == RunState::GOAL
                             ? robot_log::Level::INFO
                             : robot_log::Level::WARN,
                         TAG, "run terminal state=%u wp=%u phase=%s why=%s",
                         unsigned(state.run.state), path_log.waypoint,
                         execute_status_name(path_log.status),
                         execute_decision_name(path_log.decision));
        state.terminal_reported = true;
      }
      xQueueOverwrite(states, &state);
      vTaskDelayUntil(&wake, std::max<TickType_t>(1, pdMS_TO_TICKS(20)));
      continue;
    }
    BootstrapFrame frame{};
    loc::localization_get_bootstrap(&frame);
    feature_task_snapshot(&state.feature);
#ifdef ROBOT_LIFECYCLE_TEST
    if (forced_rhr) {
      state.resolver = state.feature.resolution.status;
      // RHR needs only raw ranges/gyro. A later real feature resolution may
      // establish the map used by goal-pose confirmation and the EKF observer.
      // Never seed either from the simulator's physical map or true pose.
      if (state.kind == loc::MapKind::UNKNOWN &&
          state.feature.resolution.status == ResolveStatus::RESOLVED &&
          state.feature.pose.tracking)
        bind_map(state.feature.resolution);
    }
#endif
    if (state.startup == StartupStatus::RESOLVING) {
      const auto seed =
          state.feature
              .resolution; // Both engines bootstrap from MAG + wall features.
      state.resolver = seed.status;
      if (seed.status == ResolveStatus::INVALID_CONFIG)
        state.startup = StartupStatus::FAULT;
      bool allow_startup_scan = selected == NavigationEngine::FEATURE;
#ifdef ROBOT_LIFECYCLE_TEST
      // This fixture measures the stationary resolver handshake only.
      allow_startup_scan = allow_startup_scan && !lifecycle_startup_only();
#endif
      if (allow_startup_scan && !startup_scan_attempted &&
          startup_wall_turn_allowed(config, state.feature, frame, now)) {
        const int64_t epoch = state.feature.candidate_epoch_us;
        if (startup_scan_cells != state.feature.candidate_cells) {
          startup_scan_cells = state.feature.candidate_cells;
          startup_scan_confirmations = 0;
        }
        if (epoch > startup_scan_frame_us) {
          startup_scan_frame_us = epoch;
          ++startup_scan_confirmations;
        }
        if (startup_scan_confirmations >= 3) {
          startup_scan_attempted = true;
          startup_motor_started = motor::motor_control_start();
          if (!startup_motor_started)
            state.startup = StartupStatus::FAULT;
          else {
            stage_us = now;
            startup_scan = startup_motor_waiting = true;
            fallback.begin_uturn();
            state.fallback = true;
            state.fallback_reason = FallbackReason::STARTUP_WALL;
            robot_log::write(
                robot_log::Level::INFO, TAG,
                "startup wall U-turn: all %u entrance candidates swept-clear",
                state.feature.candidates);
          }
        }
      } else if (!startup_scan_attempted)
        startup_scan_confirmations = 0;
      if (startup_scan) {
        motor::MotorSnapshot scan_motor{};
        const bool have_motor = motor::motor_get_state(&scan_motor) &&
                                scan_motor.timestamp_us > 0 &&
                                scan_motor.timestamp_us <= now &&
                                now - scan_motor.timestamp_us <= 50000;
        if (startup_motor_waiting) {
          motor::motor_stop();
          if (have_motor && scan_motor.status == motor::ControlStatus::IDLE)
            startup_motor_waiting = false;
          else if (now - stage_us > 2000000)
            state.startup = StartupStatus::FAULT;
        } else if (have_motor && motor::motor_begin_run()) {
          bool candidate_changed = false;
          for (unsigned v = 0; v < 4; ++v)
            candidate_changed =
                candidate_changed ||
                (state.feature.candidate_cells[v] & ~startup_scan_cells[v]);
          auto scan = fallback.update(frame, scan_motor, now, nullptr,
                                      &state.feature.heading);
          if constexpr (motion_test::enabled)
            scan = motion_test::full_scale(scan);
          state.execution = static_cast<uint8_t>(scan.status);
          state.fallback_phase = fallback.phase();
          if (state.fallback_phase != reported_phase) {
            robot_log::write(robot_log::Level::INFO, TAG,
                             "startup wall phase=%s -> %s",
                             fallback_phase_name(reported_phase),
                             fallback_phase_name(state.fallback_phase));
            reported_phase = state.fallback_phase;
          }
          // Startup only requests the qualified turn (with forward feed when
          // enabled). It never chooses a pose from the act of turning.
          if (candidate_changed || scan.status == ExecuteStatus::FAULT ||
              !motor::motor_set_twist(
                  motion_test::rolling_turns ? scan.linear_m_s : 0,
                  scan.angular_rad_s))
            state.startup = StartupStatus::FAULT;
          if (fallback.phase() == FallbackPhase::CLEAR) {
            fallback.cancel();
            motor::motor_stop();
            startup_scan = false;
            state.fallback = false;
            state.fallback_phase = reported_phase = FallbackPhase::IDLE;
            state.execution = static_cast<uint8_t>(ExecuteStatus::IDLE);
            robot_log::write(robot_log::Level::INFO, TAG,
                             "startup wall U-turn complete; awaiting fresh "
                             "pose confirmation");
          }
        } else
          motor::motor_stop();
        if (state.startup == StartupStatus::FAULT) {
          motor::motor_stop();
          fallback.cancel();
          startup_scan = false;
        }
      } else if (startup_motor_started)
        motor::motor_stop();
      if (!startup_scan && state.startup != StartupStatus::FAULT &&
          seed.status == ResolveStatus::RESOLVED &&
          state.feature.pose.tracking) {
        stage_us = now;
        bind_map(seed);
        // EKF observes side-by-side; its handshake is not a motion gate
        // for the independently measured feature engine.
        if (selected == NavigationEngine::FEATURE)
          state.startup =
              (startup_motor_started || motor::motor_control_start())
                  ? StartupStatus::STARTING_MOTOR
                  : StartupStatus::FAULT;
        else
          state.startup = state.ekf_started ? StartupStatus::INITIALIZING_EKF
                                            : StartupStatus::FAULT;
      }
    } else if (state.startup == StartupStatus::INITIALIZING_EKF &&
               loc::localization_ready()) {
      stage_us = now;
      state.startup = motor::motor_control_start()
                          ? StartupStatus::STARTING_MOTOR
                          : StartupStatus::FAULT;
    } else if (state.startup == StartupStatus::STARTING_MOTOR) {
      motor::motor_stop();
      motor::MotorSnapshot m{};
      if (motor::motor_get_state(&m)) {
        if (m.status == motor::ControlStatus::DRIVER_FAULT ||
            m.status == motor::ControlStatus::TIMING_FAULT ||
            m.status == motor::ControlStatus::INVALID_COMMAND)
          state.startup = StartupStatus::FAULT;
        else if (m.status == motor::ControlStatus::IDLE)
          state.startup = StartupStatus::READY;
      }
    } else if (state.startup == StartupStatus::READY) {
      loc::PoseSnapshot p{};
      motor::MotorSnapshot m{};
      GoalCommand command{};
      const bool have_pose = loc::localization_get_pose(&p);
      motor::motor_get_state(&m);
      if (!fallback_latched || selected == NavigationEngine::FEATURE)
        state.ekf = have_pose ? p.health : loc::FusionHealth::FAILED;
      if (selected == NavigationEngine::FEATURE) {
        p = state.feature.pose;
        if (state.feature.contradiction_latched && !fallback_latched) {
          fallback_latched = true;
          state.fallback_reason = FallbackReason::FEATURE_CONTRADICTION;
          robot_log::write(
              robot_log::Level::WARN, TAG,
              "FEATURE contradiction latched: frames=%u; RHR armed",
              state.feature.contradiction_frames);
        }
      }
      if (xQueueReceive(commands, &command, 0) == pdTRUE) {
#if NAV_TURN_GUARD
        turn_guard.cancel();
        turn_relocation.cancel();
#endif
        executor.cancel();
        fallback.cancel();
        active = false;
        using_fallback = false;
        ending = false;
        inactive_status = ExecuteStatus::IDLE;
        inactive_fault = ExecuteFault::NONE;
        failure_reported = false;
        path_log = {};
        motor::motor_stop();
        feature_task_target({});
        end.begin({}, now);
        if (!command.cancel) {
          goal = command.goal;
          goal_us = now;
          end.begin(goal, goal_us);
          if (fallback_latched ||
              (selected == NavigationEngine::EKF &&
               (state.ekf == loc::FusionHealth::FAILED ||
                state.ekf == loc::FusionHealth::CONTRADICTING)))
            active = true;
          else {
            active = executor.begin(map, goal, p, now);
            log_route(executor, p, goal, active);
            if (!active) {
              inactive_status = executor.status();
              inactive_fault = executor.fault();
            }
          }
        } else
          robot_log::write(robot_log::Level::INFO, TAG,
                           "goal cancelled; stopping");
      }
      bool fallback_started = false;
      if (!ending && active &&
          (fallback_latched ||
           (selected == NavigationEngine::EKF &&
            (state.ekf == loc::FusionHealth::FAILED ||
             state.ekf == loc::FusionHealth::CONTRADICTING))) &&
          !using_fallback) {
#if NAV_TURN_GUARD
        turn_guard.cancel();
        turn_relocation.cancel();
#endif
        executor.cancel();
        motor::motor_stop();
        feature_task_target({});
        if (selected == NavigationEngine::EKF) {
          loc::localization_stop_fusion();
          state.fallback_reason = state.ekf == loc::FusionHealth::FAILED
                                      ? FallbackReason::EKF_FAILED
                                      : FallbackReason::EKF_CONTRADICTION;
          robot_log::write(robot_log::Level::WARN, TAG,
                           "right-hand fallback: EKF=%u", unsigned(state.ekf));
        }
#ifdef ROBOT_LIFECYCLE_TEST
        else if (forced_rhr)
          robot_log::write(robot_log::Level::WARN, TAG,
                           "right-hand fallback: forced lifecycle test");
#endif
        else
          robot_log::write(robot_log::Level::WARN, TAG,
                           "right-hand fallback: FEATURE contradiction");
        fallback.begin();
        using_fallback = fallback_latched = fallback_started = true;
      }
      DriveRequest drive{};
      bool stop_limited = false;
      drive.status = inactive_status;
      drive.fault = inactive_fault;
      if (active) {
        // This is the first command generated for the localization/path run,
        // including a zero TURN/PAUSED command. Startup/idle stops do not arm.
        if (!motor::motor_begin_run())
          continue;
        if (!ending && using_fallback &&
            end.pose_at_center(state.feature.pose, now)) {
          // A fresh independent goal pose ends RHR exploration. The executor
          // still owns the final stopped/gyro/pose dwell before GOAL.
#if NAV_TURN_GUARD
          turn_guard.cancel();
          turn_relocation.cancel();
#endif
          executor.cancel();
          fallback.cancel();
          motor::motor_stop();
          ending = true;
          using_fallback = false;
          robot_log::write(
              robot_log::Level::INFO, TAG,
              "goal pose reached; settling final center approach");
          const bool planned =
              executor.begin(map, goal, state.feature.pose, now);
          log_route(executor, state.feature.pose, goal, planned);
          if (!planned)
            active = false;
        }
        if (ending)
          p = state.feature.pose;
        bool rhr_turn_inhibited = false;
#if NAV_TURN_GUARD
        rhr_turn_inhibited = turn_guard.busy();
        // The path executor can hold its state while the guard owns motion.
        // RHR must keep consuming gyro frames: skipping updates here creates
        // an artificial IMU gap when the guard releases a pending pivot.
        if (turn_relocation.busy())
          drive = {};
        else if (turn_guard.busy() &&
                 (!using_fallback || fallback.phase() != FallbackPhase::TURN))
          drive = guarded_request;
        else
#endif
          drive =
              // Observe a fresh frame after engine handover before choosing
              // an RHR turn. The triggering contradiction is not turn proof.
              !active || (fallback_started && motion_test::nonstop_only)
                  ? DriveRequest{}
              : using_fallback
                  ? fallback.update(frame, m, now, &state.feature.pose,
                                    &state.feature.heading, rhr_turn_inhibited,
#if NAV_TURN_GUARD
                                    turn_guard.pivot_authorized()
#else
                                  false
#endif
                                        )
                  : executor.update(p, m, frame, now,
                                    selected == NavigationEngine::FEATURE ||
                                            ending
                                        ? &state.feature.heading
                                        : nullptr,
#if NAV_TURN_GUARD
                                    turn_guard.pivot_authorized()
#else
                                  false
#endif
                    );
#if NAV_TURN_GUARD
        if (!active)
          turn_relocation.cancel();
        if (turn_relocation.busy()) {
          drive =
              turn_relocation.update(map, state.feature.pose, frame, m, now);
        } else {
          if (!using_fallback || fallback.phase() != FallbackPhase::TURN)
            rhr_startup_turn_authorized = false;
          if (!rhr_startup_turn_authorized && using_fallback && active &&
              !state.feature.pose.tracking &&
              fallback.phase() == FallbackPhase::TURN &&
              drive.angular_rad_s != 0 &&
              startup_wall_turn_allowed(config, state.feature, frame, now)) {
            // The same startup-only all-hypothesis swept-circle proof used by
            // FEATURE permits this centered dead-end U-turn without a unique
            // pose. It never permits the guard's unlocalized reverse
            // correction.
            rhr_startup_turn_authorized = true;
            robot_log::write(robot_log::Level::INFO, TAG,
                             "RHR startup U-turn: all hypotheses clear");
          }
          if (!active ||
              drive.status == ExecuteStatus::FAULT ||
              drive.status == ExecuteStatus::BLOCKED) {
            rhr_startup_turn_authorized = false;
            turn_guard.cancel();
            turn_relocation.cancel();
          } else if (rhr_startup_turn_authorized) {
            turn_guard.cancel();
            turn_relocation.cancel();
          } else if (using_fallback && !state.feature.pose.tracking &&
                     fallback.phase() == FallbackPhase::CENTER &&
                     drive.linear_m_s == 0 && drive.angular_rad_s == 0 &&
                     drive.diagnostics.decision ==
                         ExecuteDecision::WAIT_TURN_CLEARANCE) {
            // A stationary range-based center wait must keep seeing fresh
            // clearance. The tracked-pose guard cannot correct this posture;
            // holding CENTER here would prevent the startup proof from running.
            turn_guard.cancel();
            turn_relocation.cancel();
          } else {
            if (!turn_guard.busy())
              guarded_request = drive;
            else if (using_fallback)
              drive = guarded_request;
            drive = turn_guard.update(map, state.feature.pose, frame, m, drive,
                                      now, &state.feature.heading);
          }
          if (turn_guard.phase() != reported_guard) {
            robot_log::write(
                turn_guard.phase() == TurnGuardPhase::LEAN ||
                        turn_guard.phase() == TurnGuardPhase::BLOCKED
                    ? robot_log::Level::WARN
                    : robot_log::Level::INFO,
                TAG, "turn guard %s -> %s side center L/R=%.3f/%.3f",
                turn_guard_phase_name(reported_guard),
                turn_guard_phase_name(turn_guard.phase()),
                double(turn_guard.left_clearance_m()),
                double(turn_guard.right_clearance_m()));
            reported_guard = turn_guard.phase();
          }

          if (active && turn_guard.needs_relocation(now) &&
              turn_relocation.begin(map, state.feature.pose, goal, now)) {
            const auto &recovery = turn_relocation.plan();
            robot_log::write(
                robot_log::Level::WARN, TAG,
                "turn relocation (%d,%d)->(%d,%d) reverse=%u added=%u steps=%u",
                state.feature.pose.grid_x, state.feature.pose.grid_y,
                recovery.cell.x, recovery.cell.y, unsigned(recovery.reverse),
                recovery.extra_steps, recovery.travel_steps);
            turn_guard.cancel();
            executor.cancel();
            fallback.cancel();
            reported_guard = TurnGuardPhase::IDLE;
            drive =
                turn_relocation.update(map, state.feature.pose, frame, m, now);
          }
        }
        if (turn_relocation.complete()) {
          robot_log::write(
              robot_log::Level::INFO, TAG,
              "turn relocation arrived cell=(%d,%d); resuming goal=(%d,%d)",
              state.feature.pose.grid_x, state.feature.pose.grid_y, goal.x,
              goal.y);
          turn_guard.cancel();
          if (using_fallback) {
            fallback.begin();
            drive = {};
            drive.status = ExecuteStatus::PAUSED;
          } else {
            const bool planned =
                executor.begin(map, goal, state.feature.pose, now,
                               turn_relocation.failed_cells());
            log_route(executor, state.feature.pose, goal, planned);
            drive = {};
            drive.status = planned ? ExecuteStatus::PAUSED : executor.status();
            drive.fault = executor.fault();
          }
          turn_relocation.finish();
        }
#endif
        const bool end_wait = drive.status == ExecuteStatus::DONE &&
                              !end.pose_at_center(state.feature.pose, now);
        if (end_wait && !path_log.end_wait)
          robot_log::write(
              robot_log::Level::INFO, TAG,
              "route done; waiting goal pose tracking=%u cell=(%d,%d)",
              unsigned(state.feature.pose.tracking),
              state.feature.pose.grid_x, state.feature.pose.grid_y);
        path_log.end_wait = end_wait;
        if (end_wait)
          drive.status = ExecuteStatus::PAUSED;
#if NAV_TURN_GUARD
        if (turn_relocation.busy())
          feature_task_target(turn_relocation.plan().cell);
        else
#endif
            if ((selected == NavigationEngine::FEATURE || ending) &&
                !executor.path().empty()) {
          const auto &route = executor.path();
          const unsigned index = std::min(drive.waypoint, route.count - 1);
          const auto target = route[index];
          const bool stop_required =
              index == 0 || index + 1 == route.count ||
              route[index + 1].x - target.x != target.x - route[index - 1].x ||
              route[index + 1].y - target.y != target.y - route[index - 1].y;
          feature_task_target(stop_required ? target : loc::Cell{});
          const auto &stop = state.feature.stop;
          if (!motion_test::enabled && index + 1 == route.count &&
              drive.linear_m_s > 0 && stop.valid && stop.target == target &&
              stop.timestamp_us > 0 && stop.timestamp_us <= now &&
              now - stop.timestamp_us <= 150000) {
            // Final center approach uses the measured front braking distance.
            // Corners use PathExecutor's distance approach and swept-circle
            // clearance, followed by the normal pose/gyro/dwell checks.
            const float cap =
                std::max(.005f, 2.f * (stop.distance_m - stop.allowance_m));
            stop_limited = drive.linear_m_s > cap;
            drive.linear_m_s = std::min(drive.linear_m_s, cap);
          }
        }
        if (drive.status == ExecuteStatus::DONE) {
          // Deadline wins an arrival exactly at/after 86 seconds.
          motor::motor_goal_reached();
          loc::localization_halt();
          active = false;
        } else if (drive.status == ExecuteStatus::FAULT ||
                   drive.status == ExecuteStatus::BLOCKED ||
                   drive.status == ExecuteStatus::NO_PATH) {
          motor::motor_stop();
          active = false;
          inactive_status = drive.status;
          inactive_fault = drive.fault;
        }
      }
      if constexpr (motion_test::enabled) {
        if (active && (drive.status == ExecuteStatus::TURN ||
                       drive.status == ExecuteStatus::ADVANCE ||
                       drive.status == ExecuteStatus::FALLBACK))
          drive = motion_test::full_scale(drive);
      }
      path_log.update(drive, p, m, state.feature, active, stop_limited, now);
      if (drive.status == ExecuteStatus::PAUSED &&
          state.execution != static_cast<uint8_t>(ExecuteStatus::PAUSED)) {
        robot_log::write(
            robot_log::Level::WARN, TAG,
            "path PAUSED wp=%u tracking=%u resolve=%s candidates=%u",
            drive.waypoint, unsigned(p.tracking),
            resolve_status_name(state.feature.resolution.status),
            state.feature.candidates);
      }
      if (!failure_reported && (drive.status == ExecuteStatus::FAULT ||
                                drive.status == ExecuteStatus::BLOCKED ||
                                drive.status == ExecuteStatus::NO_PATH)) {
        failure_reported = true;
        const char *status = drive.status == ExecuteStatus::FAULT ? "FAULT"
                             : drive.status == ExecuteStatus::BLOCKED
                                 ? "BLOCKED"
                                 : "NO_PATH";
        robot_log::write(robot_log::Level::ERROR, TAG,
                         "%s %s reason=%s wp=%u leg_ms=%lld",
                         using_fallback ? "RHR" : "PATH", status,
                         execute_fault_name(drive.fault), drive.waypoint,
                         static_cast<long long>(
                             using_fallback || !executor.leg_started_us()
                                 ? -1
                                 : (now - executor.leg_started_us()) / 1000));
        robot_log::write(
            robot_log::Level::ERROR, TAG,
            "pose tracking=%u age_ms=%lld cell=(%d,%d) resolve=%s",
            unsigned(p.tracking),
            static_cast<long long>(
                p.timestamp_us > 0 ? (now - p.timestamp_us) / 1000 : -1),
            p.grid_x, p.grid_y,
            resolve_status_name(state.feature.resolution.status));
        robot_log::write(robot_log::Level::ERROR, TAG,
                         "motor=%u pwm=%.3f/%.3f tof=%.3f/%.3f/%.3f",
                         unsigned(m.status), double(m.duty_left),
                         double(m.duty_right), double(frame.range_m[0]),
                         double(frame.range_m[1]), double(frame.range_m[2]));
      }
      state.end_tape_seen = state.end_tape_unresolved = false;
      state.execution = static_cast<uint8_t>(drive.status);
      state.execution_fault = static_cast<uint8_t>(drive.fault);
      state.fallback = using_fallback;
      state.fallback_phase = fallback.phase();
      if (state.fallback_phase != reported_phase) {
        robot_log::write(state.fallback_phase == FallbackPhase::PAUSED ||
                                 state.fallback_phase == FallbackPhase::FAULT
                             ? robot_log::Level::WARN
                             : robot_log::Level::INFO,
                         TAG, "right-hand phase=%s -> %s reason=%u",
                         fallback_phase_name(reported_phase),
                         fallback_phase_name(state.fallback_phase),
                         unsigned(state.fallback_reason));
        reported_phase = state.fallback_phase;
      }
      if (!motor::motor_run_state().terminal() &&
          !motor::motor_set_twist(drive.linear_m_s, drive.angular_rad_s)) {
        // A concurrent timeout/halt must retain its terminal reason.
        if (!motor::motor_run_state().terminal()) {
          motor::motor_halt();
          state.startup = StartupStatus::FAULT;
        }
#if NAV_TURN_GUARD
        turn_guard.cancel();
        turn_relocation.cancel();
#endif
        executor.cancel();
        fallback.cancel();
      }
    } else if (state.startup == StartupStatus::FAULT)
      motor::motor_stop();
    if ((state.startup == StartupStatus::INITIALIZING_EKF ||
         state.startup == StartupStatus::STARTING_MOTOR) &&
        now - stage_us > 2000000) {
      state.startup = StartupStatus::FAULT;
      motor::motor_stop();
    }
    state.run = motor::motor_run_state();
    xQueueOverwrite(states, &state);
    vTaskDelayUntil(&wake, std::max<TickType_t>(1, pdMS_TO_TICKS(20)));
  }
}
} // namespace
bool navigation_start(const ResolverConfig &c, NavigationEngine engine,
                      const FeatureConfig &features,
                      const RightHandConfig &rhr) {
  if (commands || states || halted.load(std::memory_order_acquire) ||
      motor::motor_run_state().terminal())
    return false;
  if (engine != NavigationEngine::FEATURE && engine != NavigationEngine::EKF)
    return false;
  selected = engine;
  fallback_config = rhr;
  commands = xQueueCreate(1, sizeof(GoalCommand));
  states = xQueueCreate(1, sizeof(NavigationSnapshot));
  config = c;
  TaskHandle_t handle = nullptr;
  if (!commands || !states || !feature_task_start(c, features) ||
      xTaskCreate(path_task, "path_exec", 2048, nullptr, 2, &handle) !=
          pdPASS) {
    feature_task_halt();
    if (commands)
      vQueueDelete(commands);
    if (states)
      vQueueDelete(states);
    commands = states = nullptr;
    return false;
  }
  return true;
}
bool navigation_set_goal(loc::Cell goal) {
  NavigationSnapshot s{};
  if (halted.load(std::memory_order_acquire) ||
      motor::motor_run_state().terminal() || !loc::valid_cell(goal) ||
      !navigation_get_state(&s) || s.startup != StartupStatus::READY)
    return false;
  const GoalCommand command{goal, false};
  return xQueueOverwrite(commands, &command) == pdTRUE;
}
bool navigation_cancel() {
  if (halted.load(std::memory_order_acquire) ||
      motor::motor_run_state().terminal())
    return false;
  const GoalCommand command{{}, true};
  return commands && xQueueOverwrite(commands, &command) == pdTRUE;
}
void navigation_halt() {
  halted.store(true, std::memory_order_release);
  feature_task_halt();
  motor::motor_halt();
  loc::localization_halt();
}
bool navigation_get_state(NavigationSnapshot *out) {
  if (!out || !states || xQueuePeek(states, out, 0) != pdTRUE)
    return false;
  out->run = motor::motor_run_state();
  // Report terminal status even if the path task has not been scheduled again.
  if (out->run.terminal()) {
    out->startup =
        out->run.state == RunState::TIMEOUT
            ? StartupStatus::TIMEOUT
            : (out->run.state == RunState::GOAL ? StartupStatus::DONE
                                                : StartupStatus::HALTED);
    out->execution = static_cast<uint8_t>(
        out->run.state == RunState::TIMEOUT
            ? ExecuteStatus::TIMEOUT
            : (out->run.state == RunState::GOAL ? ExecuteStatus::DONE
                                                : ExecuteStatus::HALTED));
  }
  return true;
}
} // namespace nav
