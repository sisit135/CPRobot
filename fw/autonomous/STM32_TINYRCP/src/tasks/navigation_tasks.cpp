#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/end_cell.hpp"
#include "navigation/feature_tasks.hpp"
#include "navigation/navigation.hpp"
#include "navigation/path_executor.hpp"
#include "navigation/right_hand_rule.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include <algorithm>
#include <atomic>
namespace nav {
namespace {
std::atomic<bool> halted{false};
struct GoalCommand {
  loc::Cell goal;
  bool cancel = false;
};
QueueHandle_t commands = nullptr, states = nullptr;
ResolverConfig config;
NavigationEngine selected = NavigationEngine::FEATURE;
void path_task(void *) {
  ExecutorConfig execute_config;
  execute_config.position_tolerance_m = .010f;
  PathExecutor executor(execute_config);
  RightHandRule fallback;
  EndCellGate end;
  NavigationSnapshot state;
  state.engine = selected;
  loc::MazeMap map;
  int64_t stage_us = 0, goal_us = 0;
  loc::Cell goal{};
  bool active = false, using_fallback = false, fallback_latched = false,
       ending = false, end_unresolved = false;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    const int64_t now = hal_time_us();
    state.run = motor::motor_run_state();
    if (halted.load(std::memory_order_acquire) || state.run.terminal()) {
      executor.cancel();
      fallback.cancel();
      active = false;
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
        robot_log::write(
            state.run.state == RunState::GOAL ? robot_log::Level::INFO
                                              : robot_log::Level::WARN,
            "navigation", "run terminal state=%u", unsigned(state.run.state));
        state.terminal_reported = true;
      }
      xQueueOverwrite(states, &state);
      vTaskDelayUntil(&wake, std::max<TickType_t>(1, pdMS_TO_TICKS(20)));
      continue;
    }
    BootstrapFrame frame{};
    loc::localization_get_bootstrap(&frame);
    feature_task_snapshot(&state.feature);
    if (state.startup == StartupStatus::RESOLVING) {
      const auto seed =
          state.feature
              .resolution; // Both engines bootstrap from MAG + wall features.
      state.resolver = seed.status;
      if (seed.status == ResolveStatus::INVALID_CONFIG)
        state.startup = StartupStatus::FAULT;
      if (seed.status == ResolveStatus::RESOLVED &&
          state.feature.pose.tracking) {
        stage_us = now;
        map = config.variants[static_cast<unsigned>(seed.kind)];
        state.kind = seed.kind;
        hal_navigation_resolved(map);
        state.ekf_started = loc::localization_start(map, seed);
        // EKF observes side-by-side; its handshake is not a motion gate
        // for the independently measured feature engine.
        if (selected == NavigationEngine::FEATURE)
          state.startup = motor::motor_control_start()
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
      if (!fallback_latched)
        state.ekf = have_pose ? p.health : loc::FusionHealth::FAILED;
      if (selected == NavigationEngine::FEATURE)
        p = state.feature.pose;
      if (xQueueReceive(commands, &command, 0) == pdTRUE) {
        executor.cancel();
        fallback.cancel();
        active = false;
        using_fallback = false;
        ending = false;
        end_unresolved = false;
        motor::motor_stop();
        feature_task_target({});
        end.begin({}, now);
        if (!command.cancel) {
          goal = command.goal;
          goal_us = now;
          end.begin(goal, goal_us);
          if (selected == NavigationEngine::EKF &&
              (fallback_latched || state.ekf == loc::FusionHealth::FAILED ||
               state.ekf == loc::FusionHealth::CONTRADICTING))
            active = true;
          else
            active = executor.begin(map, goal, p, now);
        }
      }
      if (!ending && selected == NavigationEngine::EKF && active &&
          (fallback_latched || state.ekf == loc::FusionHealth::FAILED ||
           state.ekf == loc::FusionHealth::CONTRADICTING) &&
          !using_fallback) {
        executor.cancel();
        motor::motor_stop();
        loc::localization_stop_fusion();
        fallback.begin();
        using_fallback = fallback_latched = true;
        robot_log::write(robot_log::Level::WARN, "navigation",
                         "right-hand fallback: EKF=%u", unsigned(state.ekf));
      }
      DriveRequest drive{};
      if (active) {
        // This is the first command generated for the localization/path run,
        // including a zero TURN/PAUSED command. Startup/idle stops do not arm.
        if (!motor::motor_begin_run())
          continue;
        if (!ending && !end_unresolved &&
            end.observe(map, frame, state.feature.pose, now)) {
          // The tape is before the entrance: replan a measured final
          // approach, including when the legacy EKF is in fallback.
          executor.cancel();
          fallback.cancel();
          motor::motor_stop();
          ending = true;
          using_fallback = false;
          if (!executor.begin(map, goal, state.feature.pose, now))
            active = false;
        }
        const auto &tape = frame.end_tape;
        if (!ending && tape.id && tape.first_us >= goal_us &&
            tape.first_us > 0 && tape.confirmed_us >= tape.first_us &&
            tape.confirmed_us <= now && now - tape.confirmed_us <= 150000)
          end_unresolved =
              true; // Never drive blindly past an unassociated end cue.
        if (ending)
          p = state.feature.pose;
        drive = !active ? DriveRequest{}
                : using_fallback
                    ? fallback.update(frame, m, now)
                    : executor.update(p, m, frame, now,
                                      selected == NavigationEngine::FEATURE ||
                                              ending
                                          ? &state.feature.heading
                                          : nullptr);
        if (end_unresolved) {
          drive = {};
          drive.status = ExecuteStatus::PAUSED;
        }
        if (drive.status == ExecuteStatus::DONE &&
            (!ending || !end.at_center(state.feature.pose, now)))
          drive.status = ExecuteStatus::PAUSED; // No tape or no center proof:
                                                // never declare success.
        if ((selected == NavigationEngine::FEATURE || ending) &&
            !executor.path().empty()) {
          const auto &route = executor.path();
          const auto target = route[std::min(drive.waypoint, route.count - 1)];
          feature_task_target(target);
          const auto &stop = state.feature.stop;
          if (drive.linear_m_s > 0 && stop.valid && stop.target == target &&
              stop.timestamp_us > 0 && stop.timestamp_us <= now &&
              now - stop.timestamp_us <= 150000) {
            // Slow as the measured remaining distance approaches the
            // coast allowance. PathExecutor still requires settled
            // feature evidence before claiming waypoint/goal arrival.
            drive.linear_m_s =
                std::min(drive.linear_m_s,
                         std::max(.005f, stop.distance_m - stop.allowance_m));
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
        }
      }
      state.end_tape_seen = end.latched();
      state.end_tape_unresolved = end_unresolved;
      state.execution = static_cast<uint8_t>(drive.status);
      state.fallback = using_fallback;
      state.fallback_phase = fallback.phase();
      if (!motor::motor_run_state().terminal() &&
          !motor::motor_set_twist(drive.linear_m_s, drive.angular_rad_s)) {
        // A concurrent timeout/halt must retain its terminal reason.
        if (!motor::motor_run_state().terminal()) {
          motor::motor_halt();
          state.startup = StartupStatus::FAULT;
        }
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
                      const FeatureConfig &features) {
  if (commands || states || halted.load(std::memory_order_acquire) ||
      motor::motor_run_state().terminal())
    return false;
  if (engine != NavigationEngine::FEATURE && engine != NavigationEngine::EKF)
    return false;
  selected = engine;
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
