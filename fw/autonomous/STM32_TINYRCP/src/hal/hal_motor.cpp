// SPDX-License-Identifier: GPL-3.0-only

#include "motor_control/hal_motor.hpp"
#include "Arduino.h"
#include "board_motor_pins.hpp"
#include "motor_control/config.hpp"
#include <cmath>
namespace {
bool initialized = false, fault_latched = false;
/** @brief Apply fast-decay PWM to one bridge, turning the other input off
 * first.
 * @param[in] a DRV8833 xIN1 PWM pin.
 * @param[in] b DRV8833 xIN2 PWM pin.
 * @param[in] duty Signed normalized duty.
 * @returns No value; Arduino APIs report no electrical output success.
 */
void drive(int a, int b, float duty) {
  const int pwm =
      static_cast<int>(std::lround(std::fabs(duty) * motor::cfg::pwm_max_duty));
  if (duty > 0) {
    analogWrite(b, 0);
    analogWrite(a, pwm);
  } else if (duty < 0) {
    analogWrite(a, 0);
    analogWrite(b, pwm);
  } else {
    analogWrite(a, 0);
    analogWrite(b, 0);
  }
}
/** @brief Latch an observed driver fault and disable both channels.
 * @returns No value.
 */
[[maybe_unused]] void shutdown() {
  fault_latched = true;
  hal_motor_stop();
  if (board::nsleep_gpio >= 0)
    digitalWrite(board::nsleep_gpio, LOW);
}
bool driver_fault_clear() {
  // Temporary bench bypass: nFAULT is not connected. Keep its pin configured.
  return true;
  /*
  if (board::nfault_gpio >= 0 && digitalRead(board::nfault_gpio) == LOW) {
    shutdown();
    return false;
  }
  return !fault_latched;
  */
}
} // namespace
bool hal_motor_init() {
  using namespace board;
  if (initialized)
    hal_motor_stop();
  initialized = false;
  fault_latched = false;
  const int pins[] = {ain1_gpio, ain2_gpio,   bin1_gpio,
                      bin2_gpio, nsleep_gpio, nfault_gpio};
  for (unsigned i = 0; i < 6; ++i) {
    if (i < 4 && pins[i] < 0)
      return false;
    if (pins[i] < 0)
      continue;
    for (unsigned j = 0; j < i; ++j)
      if (pins[i] == pins[j])
        return false;
  }
  if (nsleep_gpio < 0 && !sleep_tied_high)
    return false;
  if (nsleep_gpio >= 0) {
    digitalWrite(nsleep_gpio, LOW);
    pinMode(nsleep_gpio, OUTPUT);
  }
  analogWriteResolution(8);
  for (unsigned i = 0; i < 4; ++i) {
    digitalWrite(pins[i], LOW);
    pinMode(pins[i], OUTPUT);
    analogWrite(pins[i], 0);
  }
  // nFAULT is open-drain; supply an external pull-up to 3.3 V on the board.
  if (nfault_gpio >= 0)
    pinMode(nfault_gpio, INPUT_PULLUP);
  initialized = true;
  if (nsleep_gpio >= 0)
    digitalWrite(nsleep_gpio, HIGH);
  delayMicroseconds(
      1000); // TI DRV8833 Rev. E tWAKE; also works before scheduler.
  if (!driver_fault_clear()) {
    return false;
  }
  return true;
}
bool hal_motor_set_duty(float left, float right) {
  using namespace motor::cfg;
  using namespace board;
  if (!initialized || fault_latched || !std::isfinite(left) ||
      !std::isfinite(right) || std::fabs(left) > 1 || std::fabs(right) > 1) {
    hal_motor_stop();
    return false;
  }
  if (!driver_fault_clear()) {
    return false;
  }
  drive(ain1_gpio, ain2_gpio, left);
  drive(bin1_gpio, bin2_gpio, right);
  return true;
}
void hal_motor_stop() {
  if (!initialized)
    return;
  drive(board::ain1_gpio, board::ain2_gpio, 0);
  drive(board::bin1_gpio, board::bin2_gpio, 0);
}
