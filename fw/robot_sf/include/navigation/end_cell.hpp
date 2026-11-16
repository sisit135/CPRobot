// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "navigation/types.hpp"
namespace nav {
struct TapeConfig {
  // Raw clear-channel thresholds at the actual integration/gain/height.
  // Zero disables detection until white/black readings have been measured.
  uint16_t black_max = 0, white_min = 0;
  unsigned confirmations = 3;
  int64_t max_gap_us = 150000;
};
class EndTapeDetector {
public:
  explicit EndTapeDetector(TapeConfig c = {}) : cfg_(c) {}
  bool process(const loc::RgbcSample &, int64_t timestamp_us, EndTapeEvent *);

private:
  TapeConfig cfg_;
  uint32_t sequence_ = 0, event_ = 0;
  int64_t last_ = 0, first_black_ = 0;
  unsigned white_ = 0, black_ = 0;
  bool armed_ = false, on_tape_ = false;
};
struct EndCellConfig {
  // Association window around the entry edge; not distance-driven odometry.
  float tape_before_edge_m = .08f, tape_after_edge_m = .03f;
  float lateral_tolerance_m = .06f, center_tolerance_m = .012f;
};
class EndCellGate {
public:
  explicit EndCellGate(EndCellConfig c = {}) : cfg_(c) {}
  void begin(loc::Cell goal, int64_t since) {
    goal_ = goal;
    since_ = since;
    latched_ = false;
  }
  bool observe(const loc::MazeMap &, const BootstrapFrame &,
               const loc::PoseSnapshot &, int64_t now);
  bool at_center(const loc::PoseSnapshot &, int64_t now) const;
  bool latched() const { return latched_; }

private:
  EndCellConfig cfg_;
  loc::Cell goal_{};
  int64_t since_ = 0;
  uint32_t consumed_ = 0;
  bool latched_ = false;
};
} // namespace nav
