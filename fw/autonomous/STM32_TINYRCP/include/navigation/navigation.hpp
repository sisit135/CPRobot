#pragma once
#include "navigation/feature_navigation.hpp"
#include "navigation/right_hand_rule.hpp"
#include "navigation/run_deadline.hpp"
#include "navigation/types.hpp"
namespace nav {
enum class NavigationEngine : uint8_t { FEATURE, EKF };
enum class StartupStatus : uint8_t {
  RESOLVING,
  INITIALIZING_EKF,
  STARTING_MOTOR,
  READY,
  FAULT,
  TIMEOUT,
  HALTED,
  DONE
};
struct NavigationSnapshot {
  NavigationEngine engine = NavigationEngine::FEATURE;
  FeatureSnapshot feature{};
  bool ekf_started = false;
  bool end_tape_seen = false, end_tape_unresolved = false;
  StartupStatus startup = StartupStatus::RESOLVING;
  ResolveStatus resolver = ResolveStatus::WAIT_SENSORS;
  loc::MapKind kind = loc::MapKind::UNKNOWN;
  uint8_t execution = 0;
  RunSnapshot run{};
  loc::FusionHealth ekf = loc::FusionHealth::INITIALIZING;
  bool fallback = false, terminal_reported = false;
  FallbackPhase fallback_phase = FallbackPhase::IDLE;
};
/** @brief Create the path task after HAL and the bus-owner task are ready.
 * @param[in] config Firmware maps; both modes bootstrap from wall features and
 * magnetic heading.
 * @param[in] engine Runtime selection, FEATURE by default; fixed for this run.
 * @param[in] features Ray-fit thresholds and measured stop allowances.
 * @returns true on creation; false on invalid allocation or repeated startup.
 */
bool navigation_start(const ResolverConfig &config,
                      NavigationEngine engine = NavigationEngine::FEATURE,
                      const FeatureConfig &features = FeatureConfig{});
/** @brief Submit a resolved-world goal; there is no automatic goal at boot.
 * @param[in] goal Valid cell index (0..4 on each axis).
 * @returns true if queued after startup reaches READY, false otherwise.
 */
bool navigation_set_goal(loc::Cell goal);
/** @brief Cancel motion on the next path-task iteration.
 * @returns true if the cancel command was queued, false before task startup.
 */
bool navigation_cancel();
/** @brief Latch motor/path halt until reset, including before path startup.
 * @returns No value; the path task will never issue nonzero commands again.
 */
void navigation_halt();
/** @brief Peek at the latest startup/path status.
 * @param[out] out Non-null destination.
 * @returns true if a status has been published.
 */
bool navigation_get_state(NavigationSnapshot *out);
} // namespace nav
/** @brief Supply measured startup color calibration and verified map
 * transforms.
 * @param[out] config In/out default hypotheses; may remain uncalibrated.
 * @returns No value.
 */
void hal_navigation_config(nav::ResolverConfig *config);
/** @brief Install variant-specific marker associations before fusion starts.
 * @param[in] map The validated resolved world-frame map.
 * @returns No value; board implementation owns static color-reference storage.
 */
void hal_navigation_resolved(const loc::MazeMap &map);
