#include "motor_control/pid.hpp"
#include "motor_control/config.hpp"
#include <algorithm>
#include <cmath>

namespace motor {
void SpeedPID::reset(float measured) {
  integral_ = derivative_ = 0;
  previous_measurement_ = std::isfinite(measured) ? measured : 0;
  initialized_ = true;
}
float SpeedPID::update(float target, float measured, float dt) {
  // if (!std::isfinite(target) || !std::isfinite(measured) ||
  // !std::isfinite(dt) ||
  //     dt < cfg::min_dt_s || dt > cfg::max_dt_s) { reset(); return 0; }
  // if (!initialized_) reset(measured);
  // const float alpha = dt / (cfg::derivative_tau_s + dt);
  // derivative_ += alpha * ((measured - previous_measurement_) / dt -
  // derivative_); previous_measurement_ = measured; const float error = target
  // - measured; const float candidate_i = std::clamp(integral_ + cfg::ki *
  // error * dt,
  //                                     -cfg::integral_limit,
  //                                     cfg::integral_limit);
  // const float other = cfg::kp * error - cfg::kd * derivative_ +
  // cfg::feedforward * target; const float candidate_u = other + candidate_i;
  // // Permit integration if unsaturated or if error brings saturation back
  // down. if ((candidate_u >= -1 && candidate_u <= 1) ||
  //     (candidate_u > 1 && error < 0) || (candidate_u < -1 && error > 0))
  //     integral_ = candidate_i;
  // return std::clamp(other + integral_, -1.0f, 1.0f);
}
} // namespace motor
