#include "board_motor_pins.hpp"
#include "motor_control/hal_motor.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
namespace {
int pwm[64]{}, level[64]{}, resolution = 0;
unsigned wake_delay = 0;
void check_bridge() {
#ifdef ROBOT_EXAMPLE_MOTOR_PINS
  // Verify the whole write sequence, including reversals, avoids both high.
  assert(!(pwm[board::ain1_gpio] && pwm[board::ain2_gpio]));
  assert(!(pwm[board::bin1_gpio] && pwm[board::bin2_gpio]));
#endif
}
void stopped() {
#ifdef ROBOT_EXAMPLE_MOTOR_PINS
  assert(!pwm[board::ain1_gpio] && !pwm[board::ain2_gpio] &&
         !pwm[board::bin1_gpio] && !pwm[board::bin2_gpio]);
#endif
}
} // namespace
void pinMode(int pin, int) { assert(pin >= 0 && pin < 64); }
void digitalWrite(int pin, int value) {
  assert(pin >= 0 && pin < 64);
  level[pin] = value;
#ifdef ROBOT_EXAMPLE_MOTOR_PINS
  if (pin == board::nsleep_gpio && value == HIGH)
    stopped();
#endif
}
int digitalRead(int pin) {
  assert(pin >= 0 && pin < 64);
  return level[pin];
}
void analogWrite(int pin, int value) {
  assert(pin >= 0 && pin < 64 && value >= 0 && value <= 255);
  pwm[pin] = value;
  check_bridge();
}
void analogWriteResolution(int bits) { resolution = bits; }
void delayMicroseconds(unsigned int us) {
  stopped();
  wake_delay += us;
}
int main() {
  assert(!hal_motor_set_duty(.5f, .5f));
#ifdef ROBOT_EXAMPLE_MOTOR_PINS
  level[board::nfault_gpio] = HIGH;
  assert(hal_motor_init() && resolution == 8 && wake_delay == 1000);
  assert(level[board::nsleep_gpio] == HIGH);
  stopped();
  assert(hal_motor_set_duty(.5f, -.25f));
  assert(pwm[board::ain1_gpio] == 128 && pwm[board::ain2_gpio] == 0);
  assert(pwm[board::bin1_gpio] == 0 && pwm[board::bin2_gpio] == 64);
  assert(hal_motor_set_duty(-.5f, .25f));
  check_bridge();
  assert(pwm[board::ain1_gpio] == 0 && pwm[board::ain2_gpio] == 128);
  assert(pwm[board::bin1_gpio] == 64 && pwm[board::bin2_gpio] == 0);
  hal_motor_stop();
  stopped();
  assert(!hal_motor_set_duty(std::numeric_limits<float>::quiet_NaN(), 0));
  stopped();
  assert(!hal_motor_set_duty(1.01f, 0));
  stopped();
  assert(hal_motor_set_duty(.5f, .5f));
  level[board::nfault_gpio] = LOW;
  assert(!hal_motor_set_duty(.5f, .5f));
  stopped();
  assert(level[board::nsleep_gpio] == LOW);
  level[board::nfault_gpio] = HIGH;
  assert(!hal_motor_set_duty(.5f, .5f));
  stopped(); // Fault stays latched.
  assert(hal_motor_init());
  stopped();
  assert(hal_motor_set_duty(.5f, .5f));
  std::puts("PASS DRV8833 PWM polarity, coast, reversal writes, wake delay, "
            "invalid duty and latched nFAULT");
#else
  assert(!hal_motor_init());
  stopped();
  std::puts("PASS DRV8833 unconfigured wiring refuses initialization");
#endif
}
