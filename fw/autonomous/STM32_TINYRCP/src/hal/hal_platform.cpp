#include "platform/hal_platform.hpp"
#include <Arduino.h>
int64_t hal_time_us() {
  static uint32_t previous = 0;
  static uint64_t extended = 1;
  const uint32_t irq = __get_PRIMASK();
  __disable_irq();
  const uint32_t now = static_cast<uint32_t>(micros());
  extended += static_cast<uint32_t>(now - previous);
  previous = now;
  const int64_t result = static_cast<int64_t>(extended);
  __set_PRIMASK(irq);
  return result;
}
