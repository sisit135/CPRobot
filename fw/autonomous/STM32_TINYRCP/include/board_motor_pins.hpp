#pragma once
#include <Arduino.h>
namespace board {
// All four xIN pins must support hardware PWM for both travel directions.
// Define ROBOT_EXAMPLE_MOTOR_PINS only after matching the actual wiring.
#ifdef ROBOT_EXAMPLE_MOTOR_PINS
constexpr int ain1_gpio = PA8, ain2_gpio = PA9, bin1_gpio = PB8,
              bin2_gpio = PB9;
constexpr int nsleep_gpio = PA6, nfault_gpio = PA7;
#else
constexpr int ain1_gpio = -1, ain2_gpio = -1, bin1_gpio = -1, bin2_gpio = -1;
constexpr int nsleep_gpio = -1, nfault_gpio = -1;
#endif
// Set true only if the module holds nSLEEP at a verified valid logic high.
constexpr bool sleep_tied_high = false;
} // namespace board
