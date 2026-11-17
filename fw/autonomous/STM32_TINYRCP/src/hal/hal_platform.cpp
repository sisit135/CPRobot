// SPDX-License-Identifier: GPL-3.0-only

#include "platform/hal_platform.hpp"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_tim.h"

namespace {
bool initialized = false;
uint32_t previous = 0;
uint64_t extended = 1;

bool start_clock() {
  const uint32_t apb1 = HAL_RCC_GetPCLK1Freq();
  const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
  // RM0383: TIMPRE selects the APB timer multiplier. Do not change the
  // shared clock tree: TIM1/TIM4 still own motor PWM.
  const uint32_t frequency =
      (RCC->DCKCFGR & RCC_DCKCFGR_TIMPRE)
          ? (prescaler <= 4 ? HAL_RCC_GetHCLKFreq() : 4 * apb1)
          : (prescaler < 4 ? apb1 : 2 * apb1);
  if (frequency < 1000000 || frequency % 1000000 != 0 ||
      frequency / 1000000 > 65536)
    return false;

  // Reserve TIM5 for a free-running 32-bit, 1 MHz counter. Use STM32 LL
  // directly, avoiding Arduino HardwareTimer/MSP callbacks and SysTick.
  __HAL_RCC_TIM5_CLK_ENABLE();
  __HAL_RCC_TIM5_FORCE_RESET();
  __HAL_RCC_TIM5_RELEASE_RESET();
  LL_TIM_SetCounterMode(TIM5, LL_TIM_COUNTERMODE_UP);
  LL_TIM_SetClockDivision(TIM5, LL_TIM_CLOCKDIVISION_DIV1);
  LL_TIM_SetPrescaler(TIM5, frequency / 1000000 - 1);
  LL_TIM_SetAutoReload(TIM5, UINT32_MAX);
  LL_TIM_GenerateEvent_UPDATE(TIM5); // Load the buffered prescaler.
  LL_TIM_SetCounter(TIM5, 0);
  LL_TIM_ClearFlag_UPDATE(TIM5);
  __HAL_DBGMCU_FREEZE_TIM5(); // Debugger halts must not consume task deadlines.
  previous = 0;
  extended = 1;
  LL_TIM_EnableCounter(TIM5);
  initialized = true;
  return true;
}
} // namespace

bool hal_time_init() {
  const uint32_t irq = __get_PRIMASK();
  __disable_irq();
  const bool ready = initialized || start_clock();
  __set_PRIMASK(irq);
  return ready;
}

int64_t hal_time_us() {
  const uint32_t irq = __get_PRIMASK();
  __disable_irq();
  if (!initialized) {
    __set_PRIMASK(irq);
    return 0;
  }
  const uint32_t now = LL_TIM_GetCounter(TIM5);
  extended += static_cast<uint32_t>(now - previous);
  previous = now;
  const int64_t result = static_cast<int64_t>(extended);
  __set_PRIMASK(irq);
  return result;
}
