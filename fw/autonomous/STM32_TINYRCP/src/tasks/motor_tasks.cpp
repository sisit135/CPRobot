// SPDX-License-Identifier: GPL-3.0-only

#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/config.hpp"
#include "motor_control/hal_motor.hpp"
#include "motor_control/motor_control.hpp"
#include "motor_control/wheel_controller.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include "remote/settings.hpp"
#include <algorithm>
#include <cmath>

namespace motor {
namespace {
QueueHandle_t s_command = nullptr, s_state = nullptr;
bool s_started = false;
nav::RunDeadline s_run;
// Called under a task critical section: deadline state, queued commands and HAL
// output writes cannot race a halt/goal on this single-core MCU. Never ISR
// APIs.
void stop_terminal() {
  const WheelCommand zero{};
  if (s_command)
    (void)xQueueOverwrite(s_command, &zero);
  hal_motor_stop();
}

/** @brief Run nominal-request ramping and open-loop PWM conversion.
 * @param[in] context Unused FreeRTOS task argument.
 * @returns Never returns.
 */
void motor_task(void *context) {
  (void)context;
  // Bring-up owns initialization; duty writes still check latched HAL faults.
  bool driver_ok = true;
  WheelController controller;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    WheelCommand cmd{};
    MotorSnapshot p{};
    taskENTER_CRITICAL();
    const int64_t now = hal_time_us();
    const auto run = s_run.poll(now);
    (void)xQueuePeek(s_command, &cmd, 0);
    const auto period = std::max<TickType_t>(
        1, pdMS_TO_TICKS(controller.settings().control_period_ms));
    if (run.terminal()) {
      stop_terminal();
      loc::localization_halt();
      p.timestamp_us = now;
      p.status =
          run.state == nav::RunState::TIMEOUT
              ? ControlStatus::RUN_TIMEOUT
              : (run.state == nav::RunState::GOAL ? ControlStatus::GOAL_REACHED
                                                  : ControlStatus::HALTED);
      xQueueOverwrite(s_state, &p);
      taskEXIT_CRITICAL();
      vTaskDelayUntil(&wake, period);
      continue;
    }
    if (driver_ok) {
      p = controller.update(cmd, now);
      if (!hal_motor_set_duty(p.duty_left, p.duty_right)) {
        driver_ok = false;
        hal_motor_stop();
      }
    }
    if (!driver_ok) {
      p.timestamp_us = now;
      p.duty_left = p.duty_right = 0;
      p.status = ControlStatus::DRIVER_FAULT;
    }
    // Recheck immediately before returning to the scheduler; direct PWM
    // writes above are bounded board HAL operations, not I2C/Serial calls.
    if (s_run.poll(hal_time_us()).terminal()) {
      stop_terminal();
      p = {};
      p.timestamp_us = hal_time_us();
      p.status = ControlStatus::RUN_TIMEOUT;
    }
    xQueueOverwrite(s_state, &p);
    taskEXIT_CRITICAL();
    vTaskDelayUntil(
        &wake, std::max<TickType_t>(
                   1, pdMS_TO_TICKS(controller.settings().control_period_ms)));
  }
}
} // namespace

bool motor_control_start() {
  static_assert(configMAX_PRIORITIES > cfg::task_priority,
                "Increase configMAX_PRIORITIES");
  if (s_started || motor_run_state().terminal())
    return false;
  s_command = xQueueCreate(1, sizeof(WheelCommand));
  s_state = xQueueCreate(1, sizeof(MotorSnapshot));
  TaskHandle_t handle = nullptr;
  if (!s_command || !s_state ||
      xTaskCreate(motor_task, "motor_pwm", cfg::task_stack_words, nullptr,
                  cfg::task_priority, &handle) != pdPASS) {
    if (s_command) {
      vQueueDelete(s_command);
      s_command = nullptr;
    }
    if (s_state) {
      vQueueDelete(s_state);
      s_state = nullptr;
    }
    return false;
  }
  s_started = true;
  return true;
}
bool motor_set_wheel_speeds(float left, float right) {
  OpenLoopConfig config;
  // A transient publication cannot admit nonzero motion against mixed limits.
  if (!remote::motor_settings(&config) && (left != 0 || right != 0))
    return false;
  if (!s_started || !std::isfinite(left) || !std::isfinite(right) ||
      std::fabs(left) > config.max_wheel_rad_s ||
      std::fabs(right) > config.max_wheel_rad_s)
    return false;
  taskENTER_CRITICAL();
  const int64_t now = hal_time_us();
  auto run = s_run.poll(now);
  if (!run.terminal() && (left != 0 || right != 0)) {
    (void)s_run.arm(now);
    run = s_run.poll(now);
  }
  bool ok = false;
  if (run.terminal())
    stop_terminal();
  else {
    const WheelCommand cmd{left, right, now};
    ok = xQueueOverwrite(s_command, &cmd) == pdTRUE;
  }
  taskEXIT_CRITICAL();
  if (ok)
    robot_log::write(robot_log::Level::TRACE, "motor",
                     "enqueue L=%.3f R=%.3f rad/s t_us=%lld",
                     static_cast<double>(left), static_cast<double>(right),
                     static_cast<long long>(now));
  return ok;
}
bool motor_set_twist(float linear, float angular) {
  if (!std::isfinite(linear) || !std::isfinite(angular))
    return false;
  const float left =
      (linear - angular * cfg::track_width_m * 0.5f) / cfg::wheel_radius_m;
  const float right =
      (linear + angular * cfg::track_width_m * 0.5f) / cfg::wheel_radius_m;
  return motor_set_wheel_speeds(left, right);
}
bool motor_stop() { return motor_set_wheel_speeds(0, 0); }
bool motor_begin_run() {
  taskENTER_CRITICAL();
  const bool ok = s_started && s_run.arm(hal_time_us());
  if (!ok && s_run.poll(hal_time_us()).terminal())
    stop_terminal();
  taskEXIT_CRITICAL();
  return ok;
}
nav::RunSnapshot motor_run_state() {
  taskENTER_CRITICAL();
  const auto run = s_run.poll(hal_time_us());
  if (run.terminal())
    stop_terminal();
  taskEXIT_CRITICAL();
  return run;
}
void motor_halt() {
  taskENTER_CRITICAL();
  (void)s_run.poll(hal_time_us());
  s_run.halt();
  stop_terminal();
  taskEXIT_CRITICAL();
}
void motor_goal_reached() {
  taskENTER_CRITICAL();
  s_run.goal(hal_time_us());
  if (s_run.poll(hal_time_us()).terminal())
    stop_terminal();
  taskEXIT_CRITICAL();
}
bool motor_get_state(MotorSnapshot *out) {
  return out && s_state && xQueuePeek(s_state, out, 0) == pdTRUE;
}
} // namespace motor
