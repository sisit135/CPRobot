// SPDX-License-Identifier: GPL-3.0-only

#include "navigation/end_cell.hpp"
#include "localization/config.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>
namespace nav {
bool EndTapeDetector::process(const loc::RgbcSample &r, int64_t t,
                              EndTapeEvent *out) {
  if (!out || !r.sequence || r.sequence <= sequence_)
    return false;
  sequence_ = r.sequence;
  const bool bad = !cfg_.black_max || cfg_.white_min <= cfg_.black_max ||
                   cfg_.confirmations < 2 || cfg_.confirmations > 10 ||
                   cfg_.max_gap_us <= 0 || t <= 0 ||
                   (last_ && (t <= last_ || t - last_ > cfg_.max_gap_us)) ||
                   r.saturated;
  last_ = t;
  if (bad) {
    white_ = black_ = 0;
    armed_ = on_tape_ = false;
    return false;
  }
  if (r.clear >= cfg_.white_min) {
    on_tape_ = false;
    black_ = 0;
    if (white_ < cfg_.confirmations)
      ++white_;
    if (white_ >= cfg_.confirmations)
      armed_ = true;
    return false;
  }
  white_ = 0;
  if (r.clear > cfg_.black_max) {
    on_tape_ = false;
    black_ = 0;
    return false;
  }
  if (!armed_) {
    if (!on_tape_)
      return false; // Starting on black is not a white-to-tape entry.
    // A new acquisition on the same continuously observed tape is fresh
    // evidence, not a new entry. Keep its identity/origin while allowing a
    // brief pose-confirmation gap to recover under the unchanged age gate.
    // White/gray, saturation and acquisition gaps revoke this continuity.
    *out = {event_, first_black_, t};
    return true;
  }
  if (!black_)
    first_black_ = t;
  if (black_ < cfg_.confirmations)
    ++black_;
  if (black_ < cfg_.confirmations ||
      event_ == std::numeric_limits<uint32_t>::max())
    return false;
  armed_ = false;
  on_tape_ = true;
  black_ = 0;
  *out = {++event_, first_black_, t};
  return true;
}
namespace {
bool fresh_pose(const loc::PoseSnapshot &p, int64_t now) {
  return p.tracking && p.timestamp_us > 0 && p.timestamp_us <= now &&
         now - p.timestamp_us <= 150000 && std::isfinite(p.x_m) &&
         std::isfinite(p.y_m) && std::isfinite(p.yaw_rad);
}
} // namespace
bool EndCellGate::observe(const loc::MazeMap &map, const BootstrapFrame &f,
                          const loc::PoseSnapshot &p, int64_t now) {
  if (latched_)
    return true;
  const auto &e = f.end_tape;
  if (!e.id || e.id <= consumed_)
    return false;
  if (!loc::valid_cell(goal_) || !map.valid() || e.first_us < since_ ||
      e.first_us <= 0 || e.confirmed_us < e.first_us || e.confirmed_us > now ||
      now - e.confirmed_us > 150000) {
    consumed_ = e.id;
    return false;
  }
  // A brief resolver confirmation gap is not a geometric rejection. Keep
  // the robot stopped while waiting within the original cue/pose age bounds.
  if (!fresh_pose(p, now) ||
      std::llabs(p.timestamp_us - e.confirmed_us) > 100000)
    return false;
  consumed_ = e.id; // A geometrically rejected event cannot be replayed later.
  const int h = (int(std::lround(p.yaw_rad / (loc::kPi / 2))) % 4 + 4) % 4;
  const float yaw = h * loc::kPi / 2;
  if (std::fabs(loc::wrap_pi(p.yaw_rad - yaw)) > loc::deg2rad(20))
    return false;
  const loc::Cell direction = h == 0   ? loc::Cell{1, 0}
                              : h == 1 ? loc::Cell{0, 1}
                              : h == 2 ? loc::Cell{-1, 0}
                                       : loc::Cell{0, -1};
  const loc::Cell from{goal_.x - direction.x, goal_.y - direction.y};
  if (!map.can_step(from, goal_))
    return false;
  const float sx = p.x_m + std::cos(p.yaw_rad) * loc::cfg::color_x_body_m -
                   std::sin(p.yaw_rad) * loc::cfg::color_y_body_m;
  const float sy = p.y_m + std::sin(p.yaw_rad) * loc::cfg::color_x_body_m +
                   std::cos(p.yaw_rad) * loc::cfg::color_y_body_m;
  const float dx =
      sx - ((goal_.x + .5f) * loc::kCellM - direction.x * loc::kCellM / 2);
  const float dy =
      sy - ((goal_.y + .5f) * loc::kCellM - direction.y * loc::kCellM / 2);
  const float along = dx * direction.x + dy * direction.y;
  const float lateral = -dx * direction.y + dy * direction.x;
  latched_ = along >= -cfg_.tape_before_edge_m &&
             along <= cfg_.tape_after_edge_m &&
             std::fabs(lateral) <= cfg_.lateral_tolerance_m;
  return latched_;
}
bool EndCellGate::at_center(const loc::PoseSnapshot &p, int64_t now) const {
  return latched_ && fresh_pose(p, now) &&
         std::hypot(p.x_m - (goal_.x + .5f) * loc::kCellM,
                    p.y_m - (goal_.y + .5f) * loc::kCellM) <=
             cfg_.center_tolerance_m;
}
} // namespace nav
