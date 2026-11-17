// SPDX-License-Identifier: GPL-3.0-only
#include "stm32f4xx_hal.h"

namespace fake_clock {
FakeClockRcc rcc;
FakeClockTimer timer;
uint32_t pclk1 = 50000000, hclk = 100000000, primask = 0;
unsigned enables = 0, resets = 0;
bool frozen = false;
void enable() { ++enables; }
void reset() {
  ++resets;
  timer = {};
}
} // namespace fake_clock
