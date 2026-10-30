#pragma once
#include <cstdint>

namespace motor::cfg {
constexpr bool invert_left_motor = false, invert_right_motor = false;
constexpr int pwm_max_duty =
    255; // 8-bit analogWrite; frequency is board-owned.
constexpr float wheel_radius_m = 0.0325f, track_width_m = 0.16f;
constexpr float max_wheel_rad_s = 30.0f;  // Nominal limit, not measured speed.
constexpr float max_accel_rad_s2 = 20.0f; // Slew limit on nominal requests.
// Educational starting values only. Identify both directions of each wheel
// on the actual supply/load; battery/load changes receive no speed correction.
struct OpenLoopCalibration {
  float forward_duty_per_rad_s, reverse_duty_per_rad_s;
  float forward_offset, reverse_offset;
};
constexpr OpenLoopCalibration left_pwm{0.05f, 0.05f, 0.0f, 0.0f};
constexpr OpenLoopCalibration right_pwm{0.05f, 0.05f, 0.0f, 0.0f};
constexpr float max_duty = 0.8f;
constexpr uint32_t control_period_ms = 10;
constexpr int64_t command_timeout_us = 300000;
constexpr float min_dt_s = 0.001f, max_dt_s = 0.05f;
constexpr uint32_t task_stack_words = 1024;
constexpr unsigned task_priority = 4;
static_assert(wheel_radius_m > 0 && track_width_m > 0,
              "Wheel geometry must be positive");
static_assert(max_duty > 0 && max_duty <= 1, "PWM cap must be in (0,1]");
static_assert(left_pwm.forward_duty_per_rad_s > 0 &&
                  left_pwm.reverse_duty_per_rad_s > 0 &&
                  right_pwm.forward_duty_per_rad_s > 0 &&
                  right_pwm.reverse_duty_per_rad_s > 0,
              "PWM conversion gains must be positive");
static_assert(
    left_pwm.forward_offset >= 0 && left_pwm.forward_offset < max_duty &&
        left_pwm.reverse_offset >= 0 && left_pwm.reverse_offset < max_duty &&
        right_pwm.forward_offset >= 0 && right_pwm.forward_offset < max_duty &&
        right_pwm.reverse_offset >= 0 && right_pwm.reverse_offset < max_duty,
    "PWM offsets must be in [0,max_duty)");
} // namespace motor::cfg
