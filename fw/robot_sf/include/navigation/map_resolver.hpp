// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "navigation/types.hpp"
namespace nav {
class MapResolver {
public:
  /** @brief Configure bounded runtime variant/pose hypotheses.
   * @param[in] config Board-calibrated colors, variant maps and local POIs.
   * @returns No value; invalid configuration remains unresolved.
   */
  explicit MapResolver(const ResolverConfig &config);
  /** @brief Score fresh color/qualified MAG/three-ToF observations.
   * @param[in] frame Latest sensor-owned bootstrap snapshot.
   * @param[in] now_us Shared monotonic clock in microseconds.
   * @returns Resolution after an unambiguous winner repeats on distinct frames.
   * @note Startup never commands motion. Identical/old frames do not accumulate
   * evidence. Invalid cell POIs are skipped; end tiles do not initialize pose.
   */
  Resolution update(const BootstrapFrame &frame, int64_t now_us);
  /** @brief Retrieve the map of an accepted variant.
   * @param[in] kind Q1..Q4.
   * @returns Configured map; invalid labels return an unknown map.
   */
  loc::MazeMap map(loc::MapKind kind) const;

private:
  ResolverConfig cfg_{};
  Resolution result_{};
  uint32_t last_color_ = 0;
  int64_t last_tof_[3]{};
  int last_key_ = -1;
  unsigned stable_ = 0;
};
} // namespace nav
