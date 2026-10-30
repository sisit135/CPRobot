#pragma once
#include <cstdint>
namespace loc {
struct PoseSnapshot;
}
/** @brief Read the shared monotonic clock from task context.
 * @returns Microseconds since boot, strictly positive, in one 64-bit domain.
 * @note Extends Arduino micros(); call at least once per 32-bit wrap period.
 */
int64_t hal_time_us();
/** @brief Publish low-rate diagnostics through an optional board logger.
 * @param[in] pose Snapshot to log; do not retain a reference to it.
 * @returns No value. The supplied board hook does nothing.
 */
void hal_log_pose(const loc::PoseSnapshot &pose);
