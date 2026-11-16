// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "localization/types.hpp"
#include "navigation/types.hpp"
namespace telemetry {
/** @brief Create a low-priority SPI polling task after HAL startup.
 * @param[in] config Map resolver settings copied for a later START_LOC command.
 * @returns true if the adapter task was created; false if pins or allocation
 * fail.
 */
bool start(const nav::ResolverConfig &config);
/** @brief Replace the pending telemetry snapshot without blocking EKF or
 * logger.
 * @param[in] pose Immutable EKF snapshot.
 * @returns No value; a newer snapshot may replace this one.
 */
void submit(const loc::PoseSnapshot &pose);
} // namespace telemetry
