#pragma once
#include "localization/maze_map.hpp"
#include "navigation/types.hpp"
namespace loc {
/** @brief Start the single bus-owner task after successful HAL startup.
 * @returns true on creation; false on allocation failure or repeated startup.
 */
bool localization_sensors_start();
/** @brief Start fusion only after the path task has resolved the map.
 * @param[in] map Resolved world-frame map.
 * @param[in] seed Qualified bootstrap pose and magnetic timestamp.
 * @returns true if tasks were created; check localization_ready() for
 * handshake.
 */
bool localization_start(const MazeMap &map, const nav::Resolution &seed);
/** @brief Read the EKF initialization handshake.
 * @returns true after the fusion owner initialized and published its first
 * pose.
 */
bool localization_ready();
/** Stop EKF event processing; retain the sensor bus owner for right-hand
 * fallback. */
void localization_stop_fusion();
/** Cooperatively halt sensor, fusion and pose logger work until MCU reset.
 * Tasks quiesce at their next loop boundary; do not delete a task holding I2C.
 */
void localization_halt();
/** @brief Peek at a coherent copy of the latest bootstrap observations.
 * @param[out] out Non-null destination.
 * @returns true when the sensor task has published a frame.
 */
bool localization_get_bootstrap(nav::BootstrapFrame *out);
/** @brief Peek at the latest EKF snapshot with freshness checked.
 * @param[out] out Non-null destination.
 * @returns true if available; inspect tracking before commanding motion.
 */
bool localization_get_pose(PoseSnapshot *out);
} // namespace loc
