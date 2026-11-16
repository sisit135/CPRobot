// SPDX-License-Identifier: GPL-3.0-only

#include "navigation/map_resolver.hpp"
#include "localization/config.hpp"
#include <cmath>
#include <limits>

namespace nav {
ResolverConfig default_resolver_config() {
  ResolverConfig c;
  const auto q1 = loc::MazeMap::q1();
  for (unsigned i = 0; i < 4; ++i)
    c.variants[i] = q1.rotated(i, static_cast<loc::MapKind>(i));
  // Entrance definitions use zero-based world cells, like every map API.
  // Q1 (TopRight) POI IDs follow the configured order; other Qs rotate them.
  c.starts[0] = {{0, 4}, TileClass::BLUE_START, 15};
  c.starts[1] = {{4, 4}, TileClass::BLUE_START, 15};
  c.starts[2] = {{4, 0}, TileClass::BLUE_START, 4};
  c.start_count = 3;
  return c;
}
MapResolver::MapResolver(const ResolverConfig &c) : cfg_(c) {
  if (c.start_count > 8 || c.color_count > 6 ||
      !std::isfinite(c.range_sigma_m) || c.range_sigma_m <= 0 ||
      !std::isfinite(c.max_score) || c.max_score <= 0 ||
      !std::isfinite(c.min_margin) || c.min_margin <= 0 ||
      !std::isfinite(c.heading_tolerance_rad) || c.heading_tolerance_rad <= 0 ||
      c.heading_tolerance_rad >= loc::kPi / 4 || c.confirmations < 2 ||
      c.confirmations > 20) {
    result_.status = ResolveStatus::INVALID_CONFIG;
    return;
  }
  for (unsigned i = 0; i < 4; ++i)
    if (!c.variants[i].valid() ||
        static_cast<unsigned>(c.variants[i].kind) != i) {
      result_.status = ResolveStatus::INVALID_CONFIG;
      return;
    }
  for (unsigned i = 0; i < c.color_count; ++i) {
    const auto &p = c.colors[i];
    const float sum = p.r + p.g + p.b;
    if (!std::isfinite(sum) || p.r < 0 || p.g < 0 || p.b < 0 ||
        std::fabs(sum - 1) > 0.02f || static_cast<unsigned>(p.tile) < 1 ||
        static_cast<unsigned>(p.tile) > 6) {
      result_.status = ResolveStatus::INVALID_CONFIG;
      return;
    }
  }
}
loc::MazeMap MapResolver::map(loc::MapKind k) const {
  const unsigned i = static_cast<unsigned>(k);
  return i < 4 ? cfg_.variants[i] : loc::MazeMap{};
}
Resolution MapResolver::update(const BootstrapFrame &f, int64_t now) {
  if (result_.status == ResolveStatus::RESOLVED ||
      result_.status == ResolveStatus::INVALID_CONFIG)
    return result_;
  auto fail = [&](ResolveStatus status) {
    stable_ = 0;
    last_key_ = -1;
    result_.status = status;
    result_.confirmations = 0;
    return result_;
  };
  if (!cfg_.colors_calibrated || !cfg_.color_count)
    return fail(ResolveStatus::WAIT_CALIBRATION);
  auto fresh = [&](int64_t t) {
    return t > 0 && t <= now && now - t <= 150000;
  };
  if (!fresh(f.color_us) || !fresh(f.imu_us) || !fresh(f.mag_us) ||
      !f.magnetic_clean || !std::isfinite(f.imu.gz_rps) ||
      std::fabs(f.imu.gz_rps) > loc::deg2rad(3))
    return fail(ResolveStatus::WAIT_SENSORS);
  for (int i = 0; i < 3; ++i)
    if (!fresh(f.tof_us[i]) || !std::isfinite(f.range_m[i]) ||
        f.range_m[i] <= loc::cfg::tof_min_m ||
        f.range_m[i] >= loc::cfg::tof_max_m)
      return fail(ResolveStatus::WAIT_SENSORS);
  if (!f.color.sequence || f.color.sequence <= last_color_)
    return result_;
  for (int i = 0; i < 3; ++i)
    if (f.tof_us[i] <= last_tof_[i])
      return result_;
  last_color_ = f.color.sequence;
  for (int i = 0; i < 3; ++i)
    last_tof_[i] = f.tof_us[i];
  const float sum = float(f.color.red) + f.color.green + f.color.blue;
  TileClass tile = TileClass::UNKNOWN;
  float d1 = 1e9f, d2 = 1e9f;
  if (!f.color.saturated && f.color.clear >= 100 && sum > 0)
    for (unsigned i = 0; i < cfg_.color_count; ++i) {
      const auto &p = cfg_.colors[i];
      const float dr = f.color.red / sum - p.r, dg = f.color.green / sum - p.g,
                  db = f.color.blue / sum - p.b;
      const float d = dr * dr + dg * dg + db * db;
      if (d < d1) {
        d2 = d1;
        d1 = d;
        tile = p.tile;
      } else if (d < d2)
        d2 = d;
    }
  if (d1 > 0.0144f || d2 - d1 < 0.004f || tile == TileClass::UNKNOWN ||
      tile == TileClass::PURPLE_END || tile == TileClass::OUTLINE)
    return fail(ResolveStatus::NO_CANDIDATE);
  const auto &mag = f.mag;
  const float norm = std::sqrt(mag.mx_uT * mag.mx_uT + mag.my_uT * mag.my_uT +
                               mag.mz_uT * mag.mz_uT);
  if (!std::isfinite(norm) || norm < loc::cfg::mag_min_uT ||
      norm > loc::cfg::mag_max_uT ||
      mag.mx_uT * mag.mx_uT + mag.my_uT * mag.my_uT < 1)
    return fail(ResolveStatus::WAIT_SENSORS);
  const float yaw = loc::wrap_pi(std::atan2(-mag.my_uT, mag.mx_uT) +
                                 loc::cfg::mag_heading_offset_rad);
  float best = std::numeric_limits<float>::infinity(), second = best;
  bool seen[400]{};
  int best_key = -1;
  loc::Cell best_cell{};
  unsigned best_variant = 0;
  auto evaluate = [&](unsigned variant, loc::Cell cell,
                      unsigned local_heading) {
    if (!loc::valid_cell(cell))
      return;
    const float nominal =
        (local_heading + cfg_.turns[variant] % 4) * loc::kPi / 2;
    if (std::fabs(loc::wrap_pi(yaw - nominal)) > cfg_.heading_tolerance_rad)
      return;
    const float x = (cell.x + 0.5f) * loc::kCellM,
                y = (cell.y + 0.5f) * loc::kCellM;
    float score = 0;
    for (int sensor = 0; sensor < 3; ++sensor) {
      const auto hit = loc::trace_maze_ray(cfg_.variants[variant], x, y, yaw,
                                           loc::cfg::lidar_mounts[sensor]);
      if (hit.status != loc::RayStatus::HIT || hit.ambiguous)
        return;
      const float residual =
          (f.range_m[sensor] - hit.range) / cfg_.range_sigma_m;
      score += residual * residual;
    }
    const int key = int(variant) * 100 + cell.y * loc::kMazeN + cell.x;
    if (seen[key])
      return; // Repeated POIs are one physical candidate.
    seen[key] = true;
    if (score < best) {
      second = best;
      best = score;
      best_key = key;
      best_cell = cell;
      best_variant = variant;
    } else if (score < second)
      second = score;
  };
  for (unsigned v = 0; v < 4; ++v) {
    if (tile == TileClass::NORMAL_Q1Q3 && v % 2 != 0)
      continue;
    if (tile == TileClass::NORMAL_Q2Q4 && v % 2 != 1)
      continue;
    if (tile == TileClass::NORMAL_Q1Q3 || tile == TileClass::NORMAL_Q2Q4) {
      for (int y = 0; y < loc::kMazeN; ++y)
        for (int x = 0; x < loc::kMazeN; ++x)
          for (unsigned h = 0; h < 4; ++h)
            evaluate(v, {x, y}, h);
    } else
      for (unsigned p = 0; p < cfg_.start_count; ++p) {
        const auto &poi = cfg_.starts[p];
        if (poi.tile != tile || !loc::valid_cell(poi.cell))
          continue;
        for (unsigned h = 0; h < 4; ++h)
          if (poi.heading_mask & (1U << h))
            evaluate(v, loc::rotate_cell(poi.cell, cfg_.turns[v]), h);
      }
  }
  result_.score = best;
  result_.margin = second - best;
  if (best_key < 0 || best > cfg_.max_score)
    return fail(ResolveStatus::NO_CANDIDATE);
  if (second - best < cfg_.min_margin)
    return fail(ResolveStatus::AMBIGUOUS);
  if (best_key != last_key_) {
    stable_ = 1;
    last_key_ = best_key;
  } else
    ++stable_;
  result_.kind = static_cast<loc::MapKind>(best_variant);
  result_.cell = best_cell;
  result_.x_m = (best_cell.x + 0.5f) * loc::kCellM;
  result_.y_m = (best_cell.y + 0.5f) * loc::kCellM;
  result_.yaw_rad = yaw;
  result_.heading_us = f.mag_us;
  result_.confirmations = stable_;
  result_.status = stable_ >= cfg_.confirmations ? ResolveStatus::RESOLVED
                                                 : ResolveStatus::CONFIRMING;
  return result_;
}
} // namespace nav
