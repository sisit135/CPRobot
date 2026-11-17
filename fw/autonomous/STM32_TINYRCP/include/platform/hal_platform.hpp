// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <cstdint>
namespace loc {
struct PoseSnapshot;
}
/** @brief Reserve and start STM32 TIM5 at 1 MHz before application startup.
 * @returns false if the timer clock cannot produce exact microsecond ticks.
 * @note Idempotent. No GPIO, timer IRQ, Arduino timing or shared-clock changes.
 */
bool hal_time_init();
/** @brief Read the shared monotonic clock from task context.
 * @returns Positive microseconds since timebase initialization, or 0 before it.
 * @note Extends the free-running TIM5 counter; call at least once per 2^32 us
 * (about 71.6 minutes). TIM5 freezes while the debugger halts the CPU.
 */
int64_t hal_time_us();
/** @brief Publish low-rate diagnostics through an optional board logger.
 * @param[in] pose Snapshot to log; do not retain a reference to it.
 * @returns No value. The supplied board hook does nothing.
 */
void hal_log_pose(const loc::PoseSnapshot &pose);
