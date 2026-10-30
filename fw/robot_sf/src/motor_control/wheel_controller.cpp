#include "motor_control/wheel_controller.hpp"
#include "motor_control/config.hpp"
#include <algorithm>
#include <cmath>

namespace motor {
namespace {
/** @brief Convert a nominal request using direction-specific calibration.
 * @param[in] speed Ramped nominal wheel request in rad/s.
 * @param[in] c Calibration for one wheel.
 * @returns Signed duty capped by cfg::max_duty; exactly zero at zero request.
 */
float duty(float speed, const cfg::OpenLoopCalibration &c) {
  if (speed == 0)
    return 0;
  const float gain =
      speed > 0 ? c.forward_duty_per_rad_s : c.reverse_duty_per_rad_s;
  const float offset = speed > 0 ? c.forward_offset : c.reverse_offset;
  return std::copysign(
      std::min(cfg::max_duty, offset + gain * std::fabs(speed)), speed);
}
/** @brief Slew through zero on reversal; zero requests stop immediately.
 * @param[in] target Current ramp value.
 * @param[in] request New nominal request.
 * @param[in] step Maximum absolute increment this iteration.
 * @returns Next ramp value without crossing zero in the same iteration.
 */
float ramp(float target, float request, float step) {
  if (request == 0)
    return 0;
  if ((target > 0 && request < 0) || (target < 0 && request > 0))
    request = 0;
  return target + std::clamp(request - target, -step, step);
}
} // namespace
void WheelController::reset_control() { target_left_ = target_right_ = 0; }
MotorSnapshot WheelController::update(const WheelCommand &cmd, int64_t now) {
  MotorSnapshot p{};
  p.timestamp_us = now;
  float dt = cfg::control_period_ms * 0.001f;
  if (now <= 0 || (previous_us_ && now <= previous_us_)) {
    previous_us_ = 0;
    reset_control();
    p.status = ControlStatus::TIMING_FAULT;
    return p;
  }
  if (previous_us_)
    dt = static_cast<float>(now - previous_us_) * 1e-6f;
  previous_us_ = now;
  if (dt < cfg::min_dt_s || dt > cfg::max_dt_s) {
    reset_control();
    p.status = ControlStatus::TIMING_FAULT;
    return p;
  }
  if (cmd.timestamp_us == 0) {
    reset_control();
    return p;
  }
  if (cmd.timestamp_us < 0 || cmd.timestamp_us > now ||
      !std::isfinite(cmd.left_rad_s) || !std::isfinite(cmd.right_rad_s) ||
      std::fabs(cmd.left_rad_s) > cfg::max_wheel_rad_s ||
      std::fabs(cmd.right_rad_s) > cfg::max_wheel_rad_s) {
    reset_control();
    p.status = ControlStatus::INVALID_COMMAND;
    return p;
  }
  if (now - cmd.timestamp_us > cfg::command_timeout_us) {
    reset_control();
    p.status = ControlStatus::COMMAND_TIMEOUT;
    return p;
  }
  if (cmd.left_rad_s == 0 && cmd.right_rad_s == 0) {
    reset_control();
    return p;
  }
  const float step = cfg::max_accel_rad_s2 * dt;
  target_left_ = ramp(target_left_, cmd.left_rad_s, step);
  target_right_ = ramp(target_right_, cmd.right_rad_s, step);
  p.target_left_rad_s = target_left_;
  p.target_right_rad_s = target_right_;
  p.duty_left = duty(target_left_, cfg::left_pwm);
  p.duty_right = duty(target_right_, cfg::right_pwm);
  p.status = ControlStatus::RUNNING;
  return p;
}
} // namespace motor
