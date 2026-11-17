// SPDX-License-Identifier: GPL-3.0-only

#include "openloop_demo/openloop_demo.hpp"

#if AXI_OPENLOOP_DEMO
#include "logging/log.hpp"
#include "motor_control/hal_motor.hpp"
#include "motor_control/wheel_controller.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include <Arduino.h>
#include <algorithm>
#include <cmath>

#ifndef AXI_LOC_KEY_PIN
#error "Open-loop demo requires AXI_LOC_KEY_PIN"
#endif
static_assert(AXI_LOC_KEY_PIN >= 0, "Open-loop demo requires a LOC key");

namespace openloop_demo {
namespace {
constexpr float kDistanceM = 1.f, kForwardMps = .20f;
constexpr float kTurnSeconds = 1.f, kQuarterTurn = 1.57079632679f;
constexpr uint32_t kStackWords = 768;
bool started = false;

// Integrate only nominal ramped requests, never sensor feedback or odometry.
// Each phase stops both outputs and resets the ramp before the next phase.
bool motion(motor::WheelController &controller,
            const motor::OpenLoopConfig &settings, float left, float right,
            float distance_m, int64_t duration_us) {
  const TickType_t period =
      std::max<TickType_t>(1, pdMS_TO_TICKS(settings.control_period_ms));
  TickType_t wake = xTaskGetTickCount();
  const int64_t begin = hal_time_us();
  int64_t previous = begin;
  float distance = 0, previous_speed = 0;
  for (;;) {
    const int64_t now = hal_time_us();
    distance += previous_speed * static_cast<float>(now - previous) * 1e-6f;
    previous = now;
    const bool finished =
        distance_m > 0 ? distance >= distance_m : now - begin >= duration_us;
    const motor::WheelCommand command{finished ? 0.f : left,
                                      finished ? 0.f : right, now};
    const auto output = controller.update(command, now, settings, 1);
    if (output.status !=
        (finished ? motor::ControlStatus::IDLE : motor::ControlStatus::RUNNING))
      return false;
    if (finished) {
      hal_motor_stop();
      vTaskDelayUntil(&wake, period);
      return true;
    }
    if (!hal_motor_set_duty(output.duty_left, output.duty_right))
      return false;
    previous_speed = (output.target_left_rad_s + output.target_right_rad_s) *
                     .5f * motor::cfg::wheel_radius_m;
    vTaskDelayUntil(&wake, period);
  }
}

void task(void *) {
  // This task alone initializes and writes DRV, regardless of BUP status.
  if (!hal_motor_init()) {
    hal_motor_stop();
    robot_log::write(robot_log::Level::ERROR, "demo",
                     "DRV init failed; stopped");
    vTaskDelete(nullptr);
    return;
  }
  pinMode(AXI_LOC_KEY_PIN, INPUT_PULLUP);
  robot_log::write(robot_log::Level::INFO, "demo", "waiting for LOC key");
  unsigned low_samples = 0;
  TickType_t wake = xTaskGetTickCount();
  while (low_samples < 3) {
    low_samples = digitalRead(AXI_LOC_KEY_PIN) == LOW ? low_samples + 1 : 0;
    vTaskDelayUntil(&wake, std::max<TickType_t>(1, pdMS_TO_TICKS(20)));
  }
  // Key polling ends here: subsequent presses cannot restart/re-arm the demo.
  const motor::OpenLoopConfig settings;
  motor::WheelController controller;
  const float forward = kForwardMps / motor::cfg::wheel_radius_m;
  const float wheel_angle = kQuarterTurn * motor::cfg::track_width_m /
                            (2.f * motor::cfg::wheel_radius_m);
  // Integral of a ramp from rest: angle = w*T - w*w/(2*a).
  // Compensate the normal request slew so the nominal pivot is ~90 degrees
  // while keeping the turn's complete powered interval at one second.
  const float accel = settings.max_accel_rad_s2;
  const float turn =
      accel * (kTurnSeconds - std::sqrt(kTurnSeconds * kTurnSeconds -
                                        2.f * wheel_angle / accel));
  robot_log::write(robot_log::Level::INFO, "demo",
                   "started: repeat 1m forward + 90deg right in 1s");
  for (;;) {
    robot_log::write(robot_log::Level::INFO, "demo", "forward ~1m at %.2fm/s",
                     static_cast<double>(kForwardMps));
    if (!motion(controller, settings, forward, forward, kDistanceM, 0))
      break;
    robot_log::write(robot_log::Level::INFO, "demo", "right elbow ~90deg, 1s");
    if (!motion(controller, settings, turn, -turn, 0,
                static_cast<int64_t>(kTurnSeconds * 1000000)))
      break;
  }
  hal_motor_stop();
  robot_log::write(robot_log::Level::ERROR, "demo",
                   "driver/command/timing fault; stopped until reset");
  vTaskDelete(nullptr);
}
} // namespace

bool start() {
  static_assert(configMAX_PRIORITIES > motor::cfg::task_priority,
                "Increase configMAX_PRIORITIES");
  if (started)
    return false;
  TaskHandle_t handle = nullptr;
  started = xTaskCreate(task, "openloop_demo", kStackWords, nullptr,
                        motor::cfg::task_priority, &handle) == pdPASS;
  return started;
}
} // namespace openloop_demo
#endif // AXI_OPENLOOP_DEMO
