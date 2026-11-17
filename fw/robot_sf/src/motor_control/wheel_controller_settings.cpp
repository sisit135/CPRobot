// SPDX-License-Identifier: GPL-3.0-only

#include "motor_control/wheel_controller.hpp"
#include "remote/settings.hpp"

namespace motor {
MotorSnapshot WheelController::update(const WheelCommand &command,
                                      int64_t now) {
  OpenLoopConfig settings = settings_;
  const auto generation = remote::motor_settings(&settings);
  return update(command, now, settings, generation);
}
} // namespace motor
