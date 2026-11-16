// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>
namespace board {
constexpr int ain1_gpio = PA6, ain2_gpio = PA7;
constexpr int bin1_gpio = PB6, bin2_gpio = PB7;
constexpr int nsleep_gpio = PB8, nfault_gpio = PB9;
constexpr bool sleep_tied_high = false;
} // namespace board
