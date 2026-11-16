// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "motor_control/tt90_reference.hpp"
#include "platform_geometry.hpp"
#include <cstdint>

#ifndef ROBOT_MAX_SPEED_ROLLING_TEST
#define ROBOT_MAX_SPEED_ROLLING_TEST 0
#endif
#if ROBOT_MAX_SPEED_ROLLING_TEST != 0 && ROBOT_MAX_SPEED_ROLLING_TEST != 1
#error "ROBOT_MAX_SPEED_ROLLING_TEST must be 0 or 1"
#endif
#ifndef NAV_NONSTOP_TURN
#define NAV_NONSTOP_TURN 1
#endif
#if NAV_NONSTOP_TURN != 0 && NAV_NONSTOP_TURN != 1
#error "NAV_NONSTOP_TURN must be 0 or 1"
#endif
#if ROBOT_MAX_SPEED_ROLLING_TEST && (!defined(NAV_TURN_GUARD) || NAV_TURN_GUARD)
#error "ROBOT_MAX_SPEED_ROLLING_TEST requires NAV_TURN_GUARD=0"
#endif

namespace motor::cfg {
constexpr bool invert_left_motor = false, invert_right_motor = false;
constexpr int pwm_max_duty =
    255; // 8-bit analogWrite; frequency is board-owned.
constexpr float wheel_radius_m = tt90::wheel_radius_m;
constexpr float track_width_m = robot::geometry::track_width_m;
constexpr float wheel_mount_x_body_m = robot::geometry::wheel_mount_x_body_m;
static_assert(-wheel_mount_x_body_m + wheel_radius_m <=
                  robot::geometry::footprint_rear_m,
              "Wheel must fit within the rear footprint boundary");
static_assert(track_width_m + robot::geometry::wheel_width_m <=
                  robot::geometry::platform_width_m,
              "Tires must fit within the lateral platform boundary");
constexpr float max_duty = 1.0f;
constexpr float max_wheel_rad_s =
    tt90::no_load_speed_rad_s(max_duty); // Provisional no-load request ceiling.
constexpr float max_accel_rad_s2 = 20.0f; // Slew limit on nominal requests.
// Datasheet-based TT 1:90 starting estimate at 6 V. Identify both directions
// on the actual robot; battery/load changes receive no speed correction.
struct OpenLoopCalibration {
  float forward_duty_per_rad_s, reverse_duty_per_rad_s;
  float forward_offset, reverse_offset;
};
constexpr OpenLoopCalibration left_pwm{
    tt90::duty_per_rad_s, tt90::duty_per_rad_s, tt90::running_duty_offset,
    tt90::running_duty_offset};
constexpr OpenLoopCalibration right_pwm = left_pwm;
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
