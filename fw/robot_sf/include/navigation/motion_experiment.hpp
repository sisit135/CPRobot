// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "motor_control/config.hpp"
#include "navigation/path_executor.hpp"
#include "remote/settings.hpp"
#include <algorithm>
#include <cmath>

namespace nav::motion_test {
inline constexpr bool enabled = ROBOT_MAX_SPEED_ROLLING_TEST;
inline constexpr bool nonstop_only = NAV_NONSTOP_TURN && !enabled;
inline constexpr bool rolling_turns = enabled || nonstop_only;
inline float max_linear_m_s() {
  motor::OpenLoopConfig config;
  (void)remote::motor_settings(&config);
  return config.max_wheel_rad_s * motor::cfg::wheel_radius_m;
}
// Positive feed while turning, before scaling both wheel requests together.
inline constexpr float turn_feed_m_s = .020f;

inline DriveRequest full_scale(DriveRequest out) {
  motor::OpenLoopConfig config;
  if (!remote::motor_settings(&config))
    return {};
  const float max_linear = config.max_wheel_rad_s * motor::cfg::wheel_radius_m;
  const float half_track = motor::cfg::track_width_m * .5f;
  const float peak =
      std::max(std::fabs(out.linear_m_s - out.angular_rad_s * half_track),
               std::fabs(out.linear_m_s + out.angular_rad_s * half_track));
  if (peak > 0 && std::isfinite(peak)) {
    // Roundoff room for the public wheel-command boundary. The converter
    // independently enforces the runtime gain, offset and PWM cap.
    const float scale = max_linear * (1.f - 1e-6f) / peak;
    out.linear_m_s *= scale;
    out.angular_rad_s *= scale;
  }
  return out;
}
} // namespace nav::motion_test
