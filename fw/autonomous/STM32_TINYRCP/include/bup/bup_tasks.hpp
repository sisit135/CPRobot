#pragma once

#include <cstdint>

namespace bup {

enum class BupStatus : uint8_t { NOT_STARTED, RUNNING, SUCCEEDED, FAILED };

/** @brief Create the one-shot power-rail and sensor bring-up task.
 * @returns true when the task was created; false on allocation failure or a
 * repeated start.
 */
bool bup_task_start();

/** @brief Read the latest bring-up task status. */
BupStatus bup_task_get_status();

} // namespace bup
