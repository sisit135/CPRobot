// SPDX-License-Identifier: GPL-3.0-only

#pragma once

namespace motor {
/** @brief PID with derivative on measurement, low-pass filtering and
 * anti-windup. */
class SpeedPID {
public:
  /** @brief Clear integral and initialize derivative history.
   * @param[in] measurement Initial measured wheel speed in rad/s.
   * @returns No value.
   */
  void reset(float measurement = 0);
  /** @brief Compute normalized signed motor duty for one sample.
   * @param[in] setpoint Desired wheel angular speed in rad/s.
   * @param[in] measurement Measured wheel angular speed in rad/s.
   * @param[in] dt_s Positive sample interval in seconds, at most cfg::max_dt_s.
   * @returns Duty in [-1,1]; zero and reset state for invalid inputs.
   */
  float update(float setpoint, float measurement, float dt_s);

private:
  float integral_ = 0, derivative_ = 0, previous_measurement_ = 0;
  bool initialized_ = false;
};
} // namespace motor
