// SPDX-License-Identifier: GPL-3.0-only

#include "localization/fusion_engine.hpp"
#include "localization/config.hpp"
#include <algorithm>
#include <cmath>

namespace loc {
bool FusionEngine::initialize(const MazeMap &map, float x, float y, float yaw,
                              int64_t stamp) {
  if (!map.valid() || !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(yaw) || x < 0 || x >= cfg::map_w_m || y < 0 ||
      y >= cfg::map_h_m || stamp <= 0)
    return false;
  *this = FusionEngine{};
  ekf_.set_maze(map);
  ekf_.reset(x, y, yaw, cfg::cell_m / std::sqrt(12.f), cfg::sigma_mag_rad);
  epoch_us_ = last_heading_us_ = stamp;
  have_epoch_ = true;
  return true;
}
bool FusionEngine::advance(int64_t stamp) {
  if (!have_epoch_) {
    epoch_us_ = stamp;
    have_epoch_ = true;
    return true;
  }
  const int64_t elapsed = stamp - epoch_us_;
  if (elapsed == 0)
    return true;
  if (elapsed < 0)
    return false;
  // Do not bridge an IMU outage with the previous acceleration/gyro.
  if (!have_imu_ || elapsed > cfg::max_predict_gap_us ||
      stamp - last_imu_us_ > cfg::max_imu_hold_us) {
    ekf_.timing_gap(static_cast<float>(elapsed) * 1e-6f);
    ++stats_.timing_gaps;
  } else {
    int64_t left = elapsed;
    while (left > 0) {
      const int64_t step = std::min<int64_t>(left, 10000);
      if (!ekf_.predict(held_imu_, static_cast<float>(step) * 1e-6f))
        return false;
      left -= step;
    }
  }
  epoch_us_ = stamp;
  return true;
}

bool FusionEngine::process(const SensorEvent &e, int64_t now) {
  if (health_ == FusionHealth::FAILED || health_ == FusionHealth::CONTRADICTING)
    return false;
  if (e.timestamp_us <= 0 || e.timestamp_us > now || now < 0) {
    ++stats_.invalid;
    return false;
  }
  if (now - e.timestamp_us > cfg::max_event_age_us) {
    ++stats_.stale;
    return false;
  }
  int stream = -1;
  switch (e.kind) {
  case SensorKind::IMU:
    stream = 0;
    if (!std::isfinite(e.imu.gz_rps) || std::fabs(e.imu.gz_rps) > 30) {
      ++stats_.invalid;
      return false;
    }
    break;
  case SensorKind::MAG:
    stream = 1;
    break;
  case SensorKind::TOF:
    if (static_cast<unsigned>(e.tof_id) > 2) {
      ++stats_.invalid;
      return false;
    }
    stream = 2 + static_cast<int>(e.tof_id);
    break;
  case SensorKind::MARKER:
    stream = 5;
    break;
  default:
    ++stats_.invalid;
    return false;
  }
  if (e.timestamp_us <= last_stream_us_[stream]) {
    ++stats_.duplicate;
    return false;
  }
  if (have_epoch_ && e.timestamp_us < epoch_us_) {
    ++stats_.out_of_order;
    return false;
  }
  if (e.kind == SensorKind::MARKER && e.marker.detected) {
    if (!e.marker.observation_id ||
        e.marker.observation_id <= last_marker_id_) {
      ++stats_.duplicate;
      return false;
    }
    last_marker_id_ = e.marker.observation_id;
  }
  // This online EKF rejects delayed measurements; it does not rewind.
  last_stream_us_[stream] = e.timestamp_us;
  if (!advance(e.timestamp_us)) {
    ++stats_.invalid;
    health_ = FusionHealth::FAILED;
    return false;
  }
  bool accepted = true;
  switch (e.kind) {
  case SensorKind::IMU:
    held_imu_ = e.imu;
    last_imu_us_ = e.timestamp_us;
    have_imu_ = true;
    break;
  case SensorKind::MAG:
    accepted = e.magnetic_clean && ekf_.update_mag(e.mag);
    if (accepted) {
      ++stats_.accepted_mag;
      last_heading_us_ = e.timestamp_us;
    } else
      ++stats_.rejected_mag;
    break;
  case SensorKind::TOF: {
    int axis = -1;
    const auto id = static_cast<unsigned>(e.tof_id);
    const auto before = ekf_.pose();
    const auto hit =
        trace_maze_ray(ekf_.maze(), before.x_m, before.y_m, before.yaw_rad,
                       cfg::lidar_mounts[id], cfg::corner_range_gap_m);
    const bool contradiction =
        std::isfinite(e.range_m) && e.range_m > cfg::tof_min_m &&
        e.range_m < cfg::tof_max_m && hit.status == RayStatus::HIT &&
        !hit.ambiguous && std::isfinite(hit.range) &&
        std::fabs(e.range_m - hit.range) > .12f;
    accepted = ekf_.update_lidar(e.range_m, cfg::lidar_mounts[id], &axis);
    // Three consecutive eligible, strongly inconsistent samples on one stream.
    // Ordinary rejects, unknown edges and corner ambiguity do not count.
    contradictory_ranges_[id] =
        !accepted && contradiction ? contradictory_ranges_[id] + 1 : 0;
    if (contradictory_ranges_[id] >= 3)
      health_ = FusionHealth::CONTRADICTING;
    if (accepted) {
      ++stats_.accepted_tof;
      (axis == RobotEKF::X ? last_x_fix_us_ : last_y_fix_us_) = e.timestamp_us;
    } else
      ++stats_.rejected_tof;
    break;
  }
  case SensorKind::MARKER: {
    if (!e.marker.detected)
      break;
    accepted = ekf_.update_marker(e.marker);
    if (accepted) {
      ++stats_.accepted_marker;
      if (e.marker.geometry != MarkerGeometry::Y_LINE)
        last_x_fix_us_ = e.timestamp_us;
      if (e.marker.geometry != MarkerGeometry::X_LINE)
        last_y_fix_us_ = e.timestamp_us;
    } else
      ++stats_.rejected_marker;
    const bool qualified = std::isfinite(e.marker.confidence) &&
                           e.marker.confidence >= cfg::min_marker_confidence &&
                           e.marker.confidence <= 1 &&
                           e.marker.geometry == MarkerGeometry::CELL &&
                           e.marker.gx >= 0 && e.marker.gx < cfg::grid_n &&
                           e.marker.gy >= 0 && e.marker.gy < cfg::grid_n;
    contradictory_markers_ =
        !accepted && qualified ? contradictory_markers_ + 1 : 0;
    if (contradictory_markers_ >= 2)
      health_ = FusionHealth::CONTRADICTING;
    break;
  }
  default:
    break;
  }
  return accepted;
}

PoseSnapshot FusionEngine::snapshot(int64_t now) const {
  PoseSnapshot p = ekf_.pose();
  p.timestamp_us = epoch_us_;
  p.last_imu_us = last_imu_us_;
  p.stats = stats_;
  p.last_x_fix_us = last_x_fix_us_;
  p.last_y_fix_us = last_y_fix_us_;
  p.tracking =
      have_imu_ && now >= epoch_us_ &&
      now - last_imu_us_ <= cfg::tracking_imu_age_us && last_x_fix_us_ > 0 &&
      now - last_x_fix_us_ <= cfg::tracking_fix_age_us && last_y_fix_us_ > 0 &&
      now - last_y_fix_us_ <= cfg::tracking_fix_age_us &&
      last_heading_us_ > 0 && p.grid_x >= 0 && p.sigma_x_m <= 0.10f &&
      p.sigma_y_m <= 0.10f && p.sigma_yaw_rad <= deg2rad(20);
  p.health = health_;
  bool finite = std::isfinite(p.x_m) && std::isfinite(p.y_m) &&
                std::isfinite(p.yaw_rad) && std::isfinite(p.gyro_bias_rps);
  for (int i = 0; i < RobotEKF::N; ++i)
    for (int j = 0; j < RobotEKF::N; ++j)
      finite = finite && std::isfinite(ekf_.covariance(i, j)) &&
               (i != j || ekf_.covariance(i, j) >= 0);
  if (p.health != FusionHealth::FAILED &&
      p.health != FusionHealth::CONTRADICTING) {
    // Allow bootstrap convergence/transient outages before declaring failure.
    const int64_t imu_epoch = last_imu_us_ ? last_imu_us_ : last_heading_us_;
    const int64_t x_epoch = last_x_fix_us_ ? last_x_fix_us_ : last_heading_us_;
    const int64_t y_epoch = last_y_fix_us_ ? last_y_fix_us_ : last_heading_us_;
    if (!finite || !have_epoch_ || now < epoch_us_ ||
        (now >= imu_epoch && now - imu_epoch >= 1000000) ||
        (now >= x_epoch && now - x_epoch >= 2000000) ||
        (now >= y_epoch && now - y_epoch >= 2000000))
      p.health = FusionHealth::FAILED;
    else if (p.grid_x < 0 || p.grid_y < 0)
      p.health = FusionHealth::CONTRADICTING;
    else
      p.health = p.tracking ? FusionHealth::TRACKING
                            : (have_imu_ ? FusionHealth::DEGRADED
                                         : FusionHealth::INITIALIZING);
  }
  if (p.health == FusionHealth::FAILED ||
      p.health == FusionHealth::CONTRADICTING)
    p.tracking = false;
  p.grid_confident = p.grid_confident && p.tracking;
  return p;
}
} // namespace loc
