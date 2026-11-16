// SPDX-License-Identifier: GPL-3.0-only

#include "logging/log.hpp"
#include <cerrno>
#include <cstddef>
#if defined(__arm__) || defined(__thumb__)
#include "platform/hal_platform.hpp"
#include <Arduino.h>
#include <ctime>
#include <sys/times.h>

// ST's enabled trace macros call clock(). Bare-metal newlib needs this hook;
// use the same monotonic HAL clock as the rest of the firmware.
extern "C" clock_t _times(struct tms *usage) {
  const uint64_t now = static_cast<uint64_t>(hal_time_us());
  const clock_t ticks =
      static_cast<clock_t>((now / 1000000) * CLOCKS_PER_SEC +
                           (now % 1000000) * CLOCKS_PER_SEC / 1000000);
  if (usage) {
    usage->tms_utime = ticks;
    usage->tms_stime = usage->tms_cutime = usage->tms_cstime = 0;
  }
  return ticks;
}
#endif
extern "C" int _write(int fd, char *data, int length) {
  if (fd != 1 && fd != 2) {
    errno = EBADF;
    return -1;
  }
  if (length < 0 || (length && !data)) {
    errno = EINVAL;
    return -1;
  }
  if (!length)
    return 0;
#if defined(__arm__) || defined(__thumb__)
  // The deferred logger uses task critical sections, not ISR primitives.
  if (__get_IPSR() != 0) {
    errno = EAGAIN;
    return -1;
  }
#endif
  // A bounded short write avoids truncation in Record::message. Newlib can
  // retry the remainder. No direct UART, waits, heap, or recursive printf.
  constexpr int capacity = sizeof(robot_log::Record{}.message) - 1;
  const int count = length < capacity ? length : capacity;
  if (!robot_log::write(fd == 2 ? robot_log::Level::ERROR
                                : robot_log::Level::INFO,
                        "stdio", "%.*s", count, data)) {
    errno = EAGAIN;
    return -1;
  }
  return count;
}
