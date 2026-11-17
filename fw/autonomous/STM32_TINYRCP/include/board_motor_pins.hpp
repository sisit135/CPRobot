// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <Arduino.h>
namespace board {
// THS v2 Black Pill: TIM1 CH1/CH2 and TIM4 CH3/CH4, respectively.
// Reserve SPI1 PA4-PA7, I2C1 PB6/PB7, I2C2 PB3/PB10 and USB PA11/PA12.
constexpr int ain1_gpio = PA8, ain2_gpio = PA9, bin1_gpio = PB8, bin2_gpio = PB9;
constexpr int nsleep_gpio = PB12, nfault_gpio = PB14;//3;

// Set true only if the module holds nSLEEP at a verified valid logic high.
constexpr bool sleep_tied_high = false;
} // namespace board
