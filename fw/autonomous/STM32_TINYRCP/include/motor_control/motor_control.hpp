// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "motor_control/types.hpp"
#include "navigation/run_deadline.hpp"

namespace motor {
/** @brief Create the command/snapshot queues and the 100 Hz open-loop PWM task.
 * @returns true if created; false if already started or allocation fails.
 * @note Called by the path task after EKF initialization. Hardware
 * initialization occurs in the task; inspect motor_get_state() for driver
 * initialization or timing errors.
 */
bool motor_control_start();
/** @brief Submit nominal left/right requests for calibrated PWM conversion.
 * @param[in] left_rad_s Nominal left wheel request in rad/s; positive is
 * forward.
 * @param[in] right_rad_s Nominal right wheel request in rad/s; positive is
 * forward.
 * @returns true if queued, false if not started, nonfinite, or above speed
 * limits.
 * @note Refresh faster than the 300 ms command timeout. An invalid command is
 *       rejected without replacing the previous command. Not callable from ISR.
 *       These are open-loop requests: actual wheel speeds are not measured.
 */
bool motor_set_wheel_speeds(float left_rad_s, float right_rad_s);
/** @brief Convert a nominal differential-drive body velocity into PWM requests.
 * @param[in] linear_m_s Forward body velocity in m/s.
 * @param[in] angular_rad_s CCW body yaw rate in rad/s.
 * @returns true if queued, false on invalid input or resulting wheel-speed
 * limits.
 * @note wl=(v-omega*track/2)/radius; wr=(v+omega*track/2)/radius.
 */
bool motor_set_twist(float linear_m_s, float angular_rad_s);
/** @brief Queue a zero-velocity command for both wheels.
 * @returns true if queued, false if the controller has not started.
 * @note Processed on the next control iteration; zero command coasts both
 * motors.
 */
bool motor_stop();
/** Arm the boot-lifetime 86 s budget immediately before the first navigation
 * command. Repeated calls never extend it. Also enforced for direct nonzero
 * motor commands. */
bool motor_begin_run();
/** Poll the monotonic deadline. All terminal states reject subsequent commands.
 */
nav::RunSnapshot motor_run_state();
/** Latch halt or verified goal. HAL writes are serialized with the motor task.
 */
void motor_halt();
void motor_goal_reached();
/** @brief Copy the latest motor controller snapshot without consuming it.
 * @param[out] out Non-null destination for nominal requests, duty, and status.
 * @returns true if available, false if null/not yet published. Safe from tasks.
 * @note Check timestamp_us for freshness. Duty is the applied PWM request,
 *       not a voltage/current measurement. Not callable from ISR.
 */
bool motor_get_state(MotorSnapshot *out);
} // namespace motor
