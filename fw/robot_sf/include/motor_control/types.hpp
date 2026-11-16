// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <cstdint>
#include <type_traits>

namespace motor {
struct WheelCommand {
  // Nominal PWM conversion requests, never measured wheel speeds.
  float left_rad_s = 0, right_rad_s = 0;
  int64_t timestamp_us = 0;
};
enum class ControlStatus : uint8_t {
  IDLE,
  RUNNING,
  COMMAND_TIMEOUT,
  INVALID_COMMAND,
  TIMING_FAULT,
  DRIVER_FAULT,
  RUN_TIMEOUT,
  HALTED,
  GOAL_REACHED
};
struct MotorSnapshot {
  int64_t timestamp_us = 0;
  float target_left_rad_s = 0,
        target_right_rad_s = 0;        // Ramped nominal requests.
  float duty_left = 0, duty_right = 0; // PWM requests, not feedback.
  ControlStatus status = ControlStatus::IDLE;
};
static_assert(std::is_trivially_copyable<WheelCommand>::value, "Queue item");
static_assert(std::is_trivially_copyable<MotorSnapshot>::value, "Queue item");
} // namespace motor
