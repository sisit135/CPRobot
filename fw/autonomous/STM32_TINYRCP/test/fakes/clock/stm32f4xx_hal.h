// SPDX-License-Identifier: GPL-3.0-only
// Host-only clock register surface; not a hardware model or firmware header.
#pragma once
#include "../stm32f4xx_hal.h"

struct FakeClockRcc {
  uint32_t CFGR = 0, DCKCFGR = 0;
};
struct FakeClockTimer {
  uint32_t CNT = 0, PSC = 0, ARR = 0;
  bool enabled = false, update = false;
};
namespace fake_clock {
extern FakeClockRcc rcc;
extern FakeClockTimer timer;
extern uint32_t pclk1, hclk, primask;
extern unsigned enables, resets;
extern bool frozen;
void enable();
void reset();
} // namespace fake_clock
#define RCC (&fake_clock::rcc)
#define TIM5 (&fake_clock::timer)
constexpr uint32_t RCC_CFGR_PPRE1 = 7u << 10;
constexpr uint32_t RCC_CFGR_PPRE1_Pos = 10;
constexpr uint32_t RCC_DCKCFGR_TIMPRE = 1u << 24;
inline uint32_t HAL_RCC_GetPCLK1Freq() { return fake_clock::pclk1; }
inline uint32_t HAL_RCC_GetHCLKFreq() { return fake_clock::hclk; }
inline uint32_t __get_PRIMASK() { return fake_clock::primask; }
inline void __disable_irq() { fake_clock::primask = 1; }
inline void __set_PRIMASK(uint32_t value) { fake_clock::primask = value; }
#define __HAL_RCC_TIM5_CLK_ENABLE() fake_clock::enable()
#define __HAL_RCC_TIM5_FORCE_RESET() fake_clock::reset()
#define __HAL_RCC_TIM5_RELEASE_RESET() ((void)0)
#define __HAL_DBGMCU_FREEZE_TIM5() (fake_clock::frozen = true)
