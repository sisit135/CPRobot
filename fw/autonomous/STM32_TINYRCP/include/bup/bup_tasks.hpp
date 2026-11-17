// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>

namespace bup {

enum class BupStatus : uint8_t { NOT_STARTED, RUNNING, SUCCEEDED, FAILED };

/** @brief Create the sole HAL initialization task. Logging must already run.
 * The optional callback starts application services only after all HALs and
 * required settings pass initialization. Any failure stops outputs and latches
 * FAILED without starting subsequent steps.
 * @returns true when the task was created; false on allocation failure or a
 * repeated start.
 * AXI_OPENLOOP_DEMO builds run an inert logging task: no HAL/settings access,
 * callback invocation or motor stop. Demo motor ownership is independent.
 */
bool bup_task_start(bool (*start_services)() = nullptr);

/** @brief Read the latest bring-up task status. */
BupStatus bup_task_get_status();

} // namespace bup
