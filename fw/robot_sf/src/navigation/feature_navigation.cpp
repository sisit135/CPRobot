#include "navigation/feature_navigation.hpp"
#include "localization/config.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace nav {
namespace {
bool fresh(int64_t t, int64_t now, int64_t age) {
  return t > 0 && t <= now && now - t <= age;
}
uint8_t exits(const loc::MazeMap &m, loc::Cell c) {
  uint8_t bits = 0;
  for (auto b : {loc::NORTH, loc::EAST, loc::SOUTH, loc::WEST})
    if (m.edge(c, b) == loc::EdgeState::OPEN)
      bits |= b;
  return bits;
}
bool intersection(uint8_t b) {
  // Corners, branches and dead ends are landmarks; straight corridors are not.
  return b != (loc::NORTH | loc::SOUTH) && b != (loc::EAST | loc::WEST);
}
} // namespace
FeatureNavigation::FeatureNavigation(const ResolverConfig &m,
                                     const FeatureConfig &c)
    : maps_(m), cfg_(c) {
  valid_ =
      c.variant_mask && !(c.variant_mask & ~15) && std::isfinite(c.sigma_m) &&
      c.sigma_m > 0 && std::isfinite(c.max_score) && c.max_score > 0 &&
      std::isfinite(c.min_margin) && c.min_margin > 0 &&
      std::isfinite(c.max_translation_m_s) && c.max_translation_m_s > 0 &&
      c.max_translation_m_s <= .5f && c.confirmations >= 2 &&
      c.confirmations <= 20 && c.max_age_us > 0 && c.max_age_us <= 200000 &&
      c.max_skew_us > 0 && c.max_skew_us <= c.max_age_us &&
      std::isfinite(c.coast_allowance_m) && c.coast_allowance_m >= 0 &&
      c.coast_allowance_m <= .02f && std::isfinite(c.command_latency_s) &&
      c.command_latency_s >= 0 && c.command_latency_s <= .1f;
  for (unsigned i = 0; i < 4; ++i)
    if (c.variant_mask & (1U << i))
      valid_ =
          valid_ && m.variants[i].valid() && unsigned(m.variants[i].kind) == i;
}
FeatureSnapshot FeatureNavigation::update(const BootstrapFrame &f,
                                          int64_t now) {
  out_.stop = {};
  out_.heading = {};
  auto fail = [&](ResolveStatus s) {
    out_.resolution.status = s;
    out_.pose.tracking = false;
    out_.pose.grid_confident = false;
    out_.pose.health = loc::FusionHealth::DEGRADED;
    confirmations_ = 0;
    candidate_ = -1;
    return out_;
  };
  if (!valid_)
    return fail(ResolveStatus::INVALID_CONFIG);
  if (now <= 0 || (last_update_ && now < last_update_)) {
    heading_ = false;
    return fail(ResolveStatus::WAIT_SENSORS);
  }
  last_update_ = now;
  if (!fresh(f.imu_us, now, 100000) || !std::isfinite(f.imu.gz_rps)) {
    heading_ = false;
    return fail(ResolveStatus::WAIT_SENSORS);
  }
  if (last_imu_ && (f.imu_us < last_imu_ || f.imu_us - last_imu_ > 100000))
    heading_ = false;
  if (heading_ && f.imu_us > last_imu_)
    yaw_ =
        loc::wrap_pi(yaw_ + f.imu.gz_rps * float(f.imu_us - last_imu_) * 1e-6f);
  last_imu_ = f.imu_us;
  if (f.magnetic_clean && fresh(f.mag_us, now, 150000)) {
    const float norm =
        std::hypot(std::hypot(f.mag.mx_uT, f.mag.my_uT), f.mag.mz_uT);
    if (std::isfinite(norm) && norm >= loc::cfg::mag_min_uT &&
        norm <= loc::cfg::mag_max_uT &&
        std::hypot(f.mag.mx_uT, f.mag.my_uT) > 1) {
      yaw_ = loc::wrap_pi(std::atan2(-f.mag.my_uT, f.mag.mx_uT) +
                          loc::cfg::mag_heading_offset_rad);
      heading_ = true;
    }
  }
  if (!heading_)
    return fail(ResolveStatus::WAIT_SENSORS);
  out_.heading = {yaw_, f.imu_us, true};
  int64_t oldest = f.tof_us[0], newest = oldest;
  bool distinct = true;
  for (int i = 0; i < 3; ++i) {
    if (!fresh(f.tof_us[i], now, cfg_.max_age_us) ||
        !std::isfinite(f.range_m[i]) || f.range_m[i] <= loc::cfg::tof_min_m ||
        f.range_m[i] >= loc::cfg::tof_max_m)
      return fail(ResolveStatus::WAIT_SENSORS);
    oldest = std::min(oldest, f.tof_us[i]);
    newest = std::max(newest, f.tof_us[i]);
    distinct = distinct && f.tof_us[i] > last_tof_[i];
  }
  // Ray fitting assumes a near-simultaneous frame. During a pivot pause
  // translation fitting; the gyro still updates above for the next frame.
  if (newest - oldest > cfg_.max_skew_us ||
      std::fabs(f.imu.gz_rps) * float(now - oldest) * 1e-6f > loc::deg2rad(3))
    return fail(ResolveStatus::WAIT_SENSORS);
  if (!distinct) {
    if (!fresh(out_.pose.timestamp_us, now, cfg_.max_age_us))
      return fail(ResolveStatus::WAIT_SENSORS);
    return out_; // Never refresh pose epoch or accumulate duplicate evidence.
  }
  for (int i = 0; i < 3; ++i)
    last_tof_[i] = f.tof_us[i];
  next_ = {};
  out_.candidates = 0;
  float best = std::numeric_limits<float>::infinity(), second = best, bx = 0,
        by = 0;
  int key = -1;
  bool best_trusted = false;
  loc::Cell cell{};
  unsigned variant = 0;
  const float cs = std::cos(yaw_), sn = std::sin(yaw_);
  for (unsigned v = 0; v < 4; ++v) {
    if (!(cfg_.variant_mask & (1U << v)) ||
        (kind_ != loc::MapKind::UNKNOWN && unsigned(kind_) != v))
      continue;
    const auto &map = maps_.variants[v];
    for (int y = 0; y < loc::kMazeN; ++y)
      for (int x = 0; x < loc::kMazeN; ++x) {
        const loc::Cell c{x, y};
        const loc::Cell previous{accepted_.grid_x, accepted_.grid_y};
        if (accepted_.timestamp_us && c != previous &&
            !map.can_step(previous, c))
          continue;
        float px = (x + .5f) * loc::kCellM, py = (y + .5f) * loc::kCellM;
        bool ok = true;
        float sum[2]{};
        unsigned count[2]{};
        for (int i = 0; i < 3; ++i) {
          const auto &mount = loc::cfg::lidar_mounts[i];
          const auto hit = loc::trace_maze_ray(map, px, py, yaw_, mount);
          if (hit.status != loc::RayStatus::HIT || hit.axis < 0 ||
              hit.axis > 1) {
            ok = false;
            break;
          }
          const float ox = cs * mount.x_body_m - sn * mount.y_body_m;
          const float oy = sn * mount.x_body_m + cs * mount.y_body_m;
          const float component = hit.axis == 0
                                      ? std::cos(yaw_ + mount.yaw_body_rad)
                                      : std::sin(yaw_ + mount.yaw_body_rad);
          sum[hit.axis] +=
              hit.line_m - (hit.axis == 0 ? ox : oy) - component * f.range_m[i];
          ++count[hit.axis];
        }
        if (!ok)
          continue;
        // An unobservable candidate still competes: deleting it can make
        // an aliased variant falsely unique at an arbitrary initial yaw.
        if (count[0])
          px = sum[0] / count[0];
        if (count[1])
          py = sum[1] / count[1];
        if (px < x * loc::kCellM || px >= (x + 1) * loc::kCellM ||
            py < y * loc::kCellM || py >= (y + 1) * loc::kCellM)
          continue;
        if (accepted_.timestamp_us) {
          const float dt = float(oldest - accepted_.timestamp_us) * 1e-6f;
          if (dt <= 0 ||
              std::hypot(px - accepted_.x_m, py - accepted_.y_m) >
                  cfg_.max_translation_m_s * std::min(dt, .5f) + .025f)
            continue;
        }
        float score = 0;
        bool trusted = count[0] && count[1];
        for (int i = 0; i < 3; ++i) {
          const auto hit =
              loc::trace_maze_ray(map, px, py, yaw_, loc::cfg::lidar_mounts[i]);
          if (hit.status != loc::RayStatus::HIT) {
            ok = false;
            break;
          }
          if (hit.ambiguous)
            trusted = false;
          const float residual = (f.range_m[i] - hit.range) / cfg_.sigma_m;
          score += residual * residual;
        }
        if (!ok || score > cfg_.max_score)
          continue;
        // Cross-reference this feature with the sequence of previous
        // map-cell hypotheses, including those not yet uniquely resolved.
        float prior = 0;
        if (have_hypotheses_) {
          prior = std::numeric_limits<float>::infinity();
          for (int oy = -1; oy <= 1; ++oy)
            for (int ox = -1; ox <= 1; ++ox) {
              if (std::abs(ox) + std::abs(oy) > 1)
                continue;
              const loc::Cell previous{x + ox, y + oy};
              if (!loc::valid_cell(previous) ||
                  (previous != c && !map.can_step(previous, c)))
                continue;
              const auto &h = hypotheses_[v * 25 + previous.y * 5 + previous.x];
              if (!h.timestamp || oldest <= h.timestamp)
                continue;
              const float dt =
                  std::min(.5f, float(oldest - h.timestamp) * 1e-6f);
              if (std::hypot(px - h.x, py - h.y) <=
                  cfg_.max_translation_m_s * dt + .025f)
                prior = std::min(prior, .8f * h.cost);
            }
          if (!std::isfinite(prior))
            continue;
        }
        score += prior;
        next_[v * 25 + y * 5 + x] = {px, py, score, oldest};
        ++out_.candidates;
        if (score < best) {
          second = best;
          best = score;
          bx = px;
          by = py;
          cell = c;
          variant = v;
          key = int(v) * 25 + y * 5 + x;
          best_trusted = trusted;
        } else
          second = std::min(second, score);
      }
  }
  out_.resolution.score = best;
  out_.resolution.margin = std::isfinite(best) ? second - best : 0;
  if (key < 0)
    return fail(ResolveStatus::NO_CANDIDATE);
  hypotheses_ = next_;
  have_hypotheses_ = true;
  // Endpoint-ambiguous matches remain competitors; dropping them could
  // falsely make a geometrically repeated distant feature look unique.
  if (!best_trusted || second - best < cfg_.min_margin)
    return fail(ResolveStatus::AMBIGUOUS);
  if (key != candidate_) {
    candidate_ = key;
    confirmations_ = 1;
  } else
    ++confirmations_;
  out_.resolution = {};
  auto &r = out_.resolution;
  r.kind = static_cast<loc::MapKind>(variant);
  r.cell = cell;
  r.x_m = bx;
  r.y_m = by;
  r.yaw_rad = yaw_;
  r.score = best;
  r.margin = second - best;
  r.confirmations = confirmations_;
  r.heading_us = oldest;
  // Existing tracked cell does not need a new startup handshake each sample.
  const bool confirmed =
      confirmations_ >= cfg_.confirmations ||
      (accepted_.timestamp_us &&
       cell == loc::Cell{accepted_.grid_x, accepted_.grid_y});
  r.status = confirmed ? ResolveStatus::RESOLVED : ResolveStatus::CONFIRMING;
  if (!confirmed) {
    out_.pose.tracking = false;
    return out_;
  }
  auto &p = out_.pose;
  p = {};
  p.timestamp_us = oldest;
  p.last_imu_us = f.imu_us;
  p.x_m = bx;
  p.y_m = by;
  p.yaw_rad = yaw_;
  p.grid_x = cell.x;
  p.grid_y = cell.y;
  p.last_x_fix_us = p.last_y_fix_us = oldest;
  p.sigma_x_m = p.sigma_y_m = cfg_.sigma_m;
  p.tracking = p.grid_confident = true;
  p.health = loc::FusionHealth::TRACKING;
  kind_ = r.kind;
  const uint8_t topology = exits(maps_.variants[variant], cell);
  if (intersection(topology) && cell != last_visit_ &&
      std::hypot(bx - (cell.x + .5f) * loc::kCellM,
                 by - (cell.y + .5f) * loc::kCellM) < .04f) {
    history_[head_] = {cell, topology, oldest};
    head_ = (head_ + 1) % history_.size();
    out_.history_size =
        std::min(out_.history_size + 1, unsigned(history_.size()));
    ++out_.intersections;
    last_visit_ = cell;
    out_.last_intersection = {cell, topology, oldest};
  }
  // Permit a legitimate revisit after leaving a landmark, without counting
  // repeated samples or pauses at that same intersection as new visits.
  if (cell != last_visit_ && !intersection(topology))
    last_visit_ = {};
  if (loc::valid_cell(target_)) {
    auto &s = out_.stop;
    s.target = target_;
    s.timestamp_us = oldest;
    s.distance_m = std::hypot((target_.x + .5f) * loc::kCellM - bx,
                              (target_.y + .5f) * loc::kCellM - by);
    if (accepted_.timestamp_us && oldest > accepted_.timestamp_us)
      s.speed_m_s =
          std::min(cfg_.max_translation_m_s,
                   std::hypot(bx - accepted_.x_m, by - accepted_.y_m) /
                       (float(oldest - accepted_.timestamp_us) * 1e-6f));
    s.allowance_m =
        cfg_.coast_allowance_m +
        s.speed_m_s * (cfg_.command_latency_s + float(now - oldest) * 1e-6f);
    s.valid = true;
    s.should_stop = s.distance_m <= s.allowance_m;
  }
  accepted_ = p;
  return out_;
}
} // namespace nav
