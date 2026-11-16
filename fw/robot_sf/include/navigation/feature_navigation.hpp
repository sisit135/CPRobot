// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "navigation/types.hpp"
#include <array>
namespace nav {
// Small persistent state: last accepted feature, heading, and a bounded event
// log. Translation is remeasured from wall rays on every accepted observation.
struct IntersectionVisit {
  loc::Cell cell{};
  uint8_t exits = 0;
  int64_t timestamp_us = 0;
};
struct StopEstimate {
  loc::Cell target{};
  int64_t timestamp_us = 0;
  float distance_m = 0, speed_m_s = 0, allowance_m = 0;
  bool valid = false, should_stop = false;
};
struct FeatureSnapshot {
  Resolution resolution{};
  HeadingObservation heading{};
  loc::PoseSnapshot pose{};
  StopEstimate stop{};
  uint32_t intersections = 0;
  unsigned history_size = 0, candidates = 0;
  // Last evaluated distinct range frame; bit (y*5+x) identifies a candidate
  // world cell in each Q1..Q4 map. Sensor waits retain the last evaluated set.
  std::array<uint32_t, 4> candidate_cells{};
  int64_t candidate_epoch_us = 0;
  uint8_t excluded_ray_mask = 0; // Runtime unstable associations, L/F/R bits.
  IntersectionVisit last_intersection{};
  // Distinct qualified NO_CANDIDATE frames after the first accepted pose.
  // Count/time freeze when the latch trips. It survives subsequent matches;
  // fresh pose evidence remains usable for the guarded final approach.
  unsigned contradiction_frames = 0;
  int64_t contradiction_us = 0;
  bool contradiction_latched = false;
};
struct FeatureConfig {
  uint8_t variant_mask = 15;
  float sigma_m = .02f, max_score = 9, min_margin = 9;
  float max_translation_m_s = .25f;
  unsigned confirmations = 3;
  unsigned contradiction_limit = 3;
  int64_t max_age_us = 150000, max_skew_us = 60000;
  // Empirical zero-PWM coast bound, not a physical braking model.
  float coast_allowance_m = .010f;
  float command_latency_s = .030f;
};
class FeatureNavigation {
public:
  explicit FeatureNavigation(const ResolverConfig &,
                             const FeatureConfig & = FeatureConfig{});
  FeatureSnapshot update(const BootstrapFrame &, int64_t now_us);
  void set_stop_target(loc::Cell target) { target_ = target; }
  const std::array<IntersectionVisit, 16> &history() const { return history_; }

private:
  struct Hypothesis {
    float x = 0, y = 0, cost = 0;
    uint8_t excluded_rays = 0;
    bool trusted = false; // Uses existing padding before the timestamp.
    int64_t timestamp = 0;
  };
  // One hypothesis per (map,cell). Evidence survives ambiguous frames and
  // propagates only over known-open edges, so intersection sequences prune
  // aliases rather than resetting the resolver at every observation.
  std::array<Hypothesis, 100> hypotheses_{}, next_{};
  bool have_hypotheses_ = false;
  ResolverConfig maps_{};
  FeatureConfig cfg_{};
  FeatureSnapshot out_{};
  std::array<IntersectionVisit, 16> history_{};
  unsigned head_ = 0;
  loc::Cell target_{}, last_visit_{};
  loc::PoseSnapshot accepted_{};
  loc::MapKind kind_ = loc::MapKind::UNKNOWN;
  int64_t last_tof_[3]{}, last_imu_ = 0, last_update_ = 0;
  int64_t last_contradiction_us_ = 0;
  float yaw_ = 0, last_gyro_ = 0;
  bool heading_ = false, valid_ = false;
  int candidate_ = -1;
  unsigned confirmations_ = 0;
};
} // namespace nav
