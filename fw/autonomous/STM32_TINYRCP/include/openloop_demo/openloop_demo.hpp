// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#ifndef AXI_OPENLOOP_DEMO
#define AXI_OPENLOOP_DEMO 0
#endif
#if AXI_OPENLOOP_DEMO != 0 && AXI_OPENLOOP_DEMO != 1
#error "AXI_OPENLOOP_DEMO must be 0 or 1"
#endif

namespace openloop_demo {
/** @brief Create the sole demo motor/key task, independently of bring-up.
 * One debounced LOC press per boot starts repeated nominal 1 m straight legs
 * and clockwise 90-degree pivots lasting 1 second. No sensor/store access.
 */
bool start();
} // namespace openloop_demo
