// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "stm32f4xx_hal.h"
#include <cassert>
constexpr uint32_t LL_TIM_COUNTERMODE_UP = 0;
constexpr uint32_t LL_TIM_CLOCKDIVISION_DIV1 = 0;
inline void LL_TIM_SetCounterMode(FakeClockTimer *, uint32_t mode) {
  assert(mode == LL_TIM_COUNTERMODE_UP);
}
inline void LL_TIM_SetClockDivision(FakeClockTimer *, uint32_t division) {
  assert(division == LL_TIM_CLOCKDIVISION_DIV1);
}
inline void LL_TIM_SetPrescaler(FakeClockTimer *timer, uint32_t value) {
  timer->PSC = value;
}
inline void LL_TIM_SetAutoReload(FakeClockTimer *timer, uint32_t value) {
  timer->ARR = value;
}
inline void LL_TIM_GenerateEvent_UPDATE(FakeClockTimer *timer) {
  timer->update = true;
}
inline void LL_TIM_SetCounter(FakeClockTimer *timer, uint32_t value) {
  timer->CNT = value;
}
inline void LL_TIM_ClearFlag_UPDATE(FakeClockTimer *timer) {
  timer->update = false;
}
inline void LL_TIM_EnableCounter(FakeClockTimer *timer) {
  timer->enabled = true;
}
inline uint32_t LL_TIM_GetCounter(FakeClockTimer *timer) {
  assert(fake_clock::primask == 1);
  return timer->CNT;
}
