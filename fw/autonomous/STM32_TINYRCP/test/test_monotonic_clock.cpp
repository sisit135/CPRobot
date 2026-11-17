// SPDX-License-Identifier: GPL-3.0-only
#include "platform/hal_platform.hpp"
#include "stm32f4xx_hal.h"
#include <cassert>
#include <cstdlib>

int main(int argc, char **argv) {
  assert(argc == 3);
  const unsigned prescaler = std::strtoul(argv[1], nullptr, 0);
  const bool timpre = std::strtoul(argv[2], nullptr, 0);
  fake_clock::hclk = 96000000;
  const unsigned divider = prescaler < 4 ? 1 : 1u << (prescaler - 3);
  fake_clock::rcc.CFGR = prescaler << RCC_CFGR_PPRE1_Pos;
  fake_clock::rcc.DCKCFGR = timpre ? RCC_DCKCFGR_TIMPRE : 0;
  fake_clock::pclk1 = fake_clock::hclk / divider;
  // Initialization must fail closed without touching timer state.
  const auto good_pclk = fake_clock::pclk1;
  const auto good_hclk = fake_clock::hclk;
  fake_clock::pclk1 = fake_clock::hclk = 1;
  assert(!hal_time_init());
  assert(!fake_clock::timer.enabled && fake_clock::enables == 0);
  assert(hal_time_us() == 0 && fake_clock::primask == 0);
  fake_clock::pclk1 = good_pclk;
  fake_clock::hclk = good_hclk;
  assert(hal_time_init());
  const unsigned frequency = timpre
                                 ? (divider <= 2 ? good_hclk : 4 * good_pclk)
                                 : (divider == 1 ? good_pclk : 2 * good_pclk);
  assert(fake_clock::timer.PSC == frequency / 1000000 - 1);
  assert(fake_clock::timer.ARR == UINT32_MAX);
  assert(fake_clock::timer.enabled && !fake_clock::timer.update);
  assert(fake_clock::frozen && fake_clock::primask == 0);
  assert(hal_time_us() == 1);
  fake_clock::timer.CNT = 1000;
  assert(hal_time_us() == 1001);
  assert(hal_time_init()); // Do not reset a running timebase.
  assert(fake_clock::resets == 1 && hal_time_us() == 1001);
  fake_clock::primask = 1;
  fake_clock::timer.CNT = 1001;
  assert(hal_time_us() == 1002 && fake_clock::primask == 1);
  assert(hal_time_init() && fake_clock::primask == 1);
  fake_clock::primask = 0;
  fake_clock::timer.CNT = 0xfffffff0u;
  const auto before = hal_time_us();
  fake_clock::timer.CNT = 0x20;
  assert(hal_time_us() - before == 48);
  assert(hal_time_us() == (int64_t{1} << 32) + 0x21);
  // Same-microsecond calls must not manufacture elapsed time.
  assert(hal_time_us() == (int64_t{1} << 32) + 0x21);
}
