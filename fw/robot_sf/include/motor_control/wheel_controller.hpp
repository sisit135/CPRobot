// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "motor_control/config.hpp"
#include "motor_control/types.hpp"

namespace motor {
/** @brief Portable open-loop PWM converter, owned by the motor task. */
class WheelController {
public:
  /** @brief Ramp nominal wheel requests and convert them to signed PWM.
   * @param[in] command Latest nominal rad/s request with monotonic timestamp.
   * @param[in] now_us Current hal_time_us() value in microseconds.
   * @returns Bounded duties and command status. Zero, invalid, expired commands
   *          and invalid task timing produce immediate zero duty.
   * @note No PID, encoder input, speed estimate, odometry or stall detection.
   *       Reversal ramps through zero before applying the opposite sign. A
   *       zero crossing does not prove mechanical standstill/current decay.
   */
  MotorSnapshot update(const WheelCommand &command, int64_t now_us);
  /** @brief Use an explicitly supplied coherent calibration without a store.
   * A new nonzero generation resets the ramp; zero retains the last settings.
   * This shares the normal conversion, slew and command/timing checks.
   */
  MotorSnapshot update(const WheelCommand &command, int64_t now_us,
                       const OpenLoopConfig &settings, uint32_t generation);
  const OpenLoopConfig &settings() const { return settings_; }

private:
  /** @brief Clear the nominal request ramp after stop/fault.
   * @returns No value.
   */
  void reset_control();
  int64_t previous_us_ = 0;
  float target_left_ = 0, target_right_ = 0;
  OpenLoopConfig settings_{};
  uint32_t settings_generation_ = 0;
};
} // namespace motor
