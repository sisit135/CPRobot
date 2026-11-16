// SPDX-License-Identifier: GPL-3.0-only

#include "navigation/feature_navigation.hpp"
#include "localization/config.hpp"
#include "navigation/turn_clearance.hpp"
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
bool finite_wall_endpoint(const loc::MazeMap &map, int x, int y) {
  auto vertical = [&](int along) {
    if (along < 0 || along >= loc::kMazeN)
      return false;
    return (x == 0 ? map.edge({0, along}, loc::WEST)
                   : map.edge({x - 1, along}, loc::EAST)) ==
           loc::EdgeState::CLOSED;
  };
  auto horizontal = [&](int along) {
    if (along < 0 || along >= loc::kMazeN)
      return false;
    return (y == 0 ? map.edge({along, 0}, loc::SOUTH)
                   : map.edge({along, y - 1}, loc::NORTH)) ==
           loc::EdgeState::CLOSED;
  };
  return vertical(y - 1) != vertical(y) || horizontal(x - 1) != horizontal(x);
}
} // namespace
FeatureNavigation::FeatureNavigation(const ResolverConfig &m,
                                     const FeatureConfig &c)
    : maps_(m), cfg_(c) {
  valid_ = m.start_count > 0 && m.start_count <= 8 && c.variant_mask &&
           !(c.variant_mask & ~15) && std::isfinite(c.sigma_m) &&
           c.sigma_m > 0 && c.sigma_m <= .1f && std::isfinite(c.max_score) &&
           c.max_score > 0 && std::isfinite(c.min_margin) && c.min_margin > 0 &&
           std::isfinite(c.max_translation_m_s) && c.max_translation_m_s > 0 &&
           c.max_translation_m_s <= .5f && c.confirmations >= 2 &&
           c.confirmations <= 20 && c.contradiction_limit >= 2 &&
           c.contradiction_limit <= 20 && c.max_age_us > 0 &&
           c.max_age_us <= 200000 && c.max_skew_us > 0 &&
           c.max_skew_us <= c.max_age_us &&
           std::isfinite(c.coast_allowance_m) && c.coast_allowance_m >= 0 &&
           c.coast_allowance_m <= .02f && std::isfinite(c.command_latency_s) &&
           c.command_latency_s >= 0 && c.command_latency_s <= .1f;
  for (unsigned i = 0; i < 4; ++i)
    if (c.variant_mask & (1U << i))
      valid_ =
          valid_ && m.variants[i].valid() && unsigned(m.variants[i].kind) == i;
  for (unsigned i = 0; i < m.start_count && i < 8; ++i)
    valid_ = valid_ && loc::valid_cell(m.starts[i].cell);
}
FeatureSnapshot FeatureNavigation::update(const BootstrapFrame &f,
                                          int64_t now) {
  out_.stop = {};
  out_.heading = {};
  auto fail = [&](ResolveStatus s) {
    out_.resolution.status = s;
    out_.pose.tracking = false;
    out_.pose.grid_confident = false;
    out_.pose.health = out_.contradiction_latched
                           ? loc::FusionHealth::CONTRADICTING
                           : loc::FusionHealth::DEGRADED;
    if (s != ResolveStatus::NO_CANDIDATE && !out_.contradiction_latched) {
      out_.contradiction_frames = 0;
      last_contradiction_us_ = 0;
    }
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
  if (f.imu_us != last_imu_) {
    if (heading_)
      yaw_ = loc::wrap_pi(yaw_ + .5f * (last_gyro_ + f.imu.gz_rps) *
                                     float(f.imu_us - last_imu_) * 1e-6f);
    last_imu_ = f.imu_us;
    last_gyro_ = f.imu.gz_rps;
  }
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
  // Initial position confirmation requires a stationary heading. A bounded
  // startup wall U-turn may collect a new signature, but it must finish and
  // coast before locking a pose or accumulating runtime contradictions.
  if (!accepted_.timestamp_us && std::fabs(f.imu.gz_rps) > .05f)
    return fail(ResolveStatus::WAIT_SENSORS);
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
  out_.candidate_cells = {};
  out_.candidate_epoch_us = oldest;
  float best = std::numeric_limits<float>::infinity(), second = best, bx = 0,
        by = 0;
  int key = -1;
  bool best_trusted = false;
  loc::Cell cell{};
  unsigned variant = 0;
  const float range_tolerance = cfg_.sigma_m * std::sqrt(cfg_.max_score);
  auto evaluate = [&](unsigned v, const loc::MazeMap &map, loc::Cell local,
                      float ray_yaw, unsigned turns, bool startup,
                      bool constrain_to_cell) {
    const loc::Cell c = loc::rotate_cell(local, turns);
    const loc::Cell previous{accepted_.grid_x, accepted_.grid_y};
    if (accepted_.timestamp_us && c != previous &&
        !maps_.variants[v].can_step(previous, c))
      return;
    const float cx = (local.x + .5f) * loc::kCellM,
                cy = (local.y + .5f) * loc::kCellM;
    if (!startup && accepted_.timestamp_us) {
      const float dt =
          std::min(.5f, float(oldest - accepted_.timestamp_us) * 1e-6f);
      const float nearest_x = std::clamp(accepted_.x_m, local.x * loc::kCellM,
                                         (local.x + 1) * loc::kCellM);
      const float nearest_y = std::clamp(accepted_.y_m, local.y * loc::kCellM,
                                         (local.y + 1) * loc::kCellM);
      if (std::hypot(nearest_x - accepted_.x_m, nearest_y - accepted_.y_m) >
          cfg_.max_translation_m_s * dt + .025f)
        return;
    }
    float px = cx, py = cy;
    unsigned endpoint_seen = 0, excluded_rays = 0;
    if (!startup && accepted_.timestamp_us) {
      // Use measured history only to choose nearby wall associations. Every
      // accepted coordinate must still be remeasured by fresh wall rays.
      const auto &h = hypotheses_[v * 25 + c.y * 5 + c.x];
      endpoint_seen =
          h.timestamp
              ? h.excluded_rays
              : hypotheses_[v * 25 + previous.y * 5 + previous.x].excluded_rays;
      px = h.timestamp && h.trusted ? h.x : accepted_.x_m;
      py = h.timestamp && h.trusted ? h.y : accepted_.y_m;
      constexpr float inset = 1e-5f;
      px = std::clamp(px, local.x * loc::kCellM + inset,
                      (local.x + 1) * loc::kCellM - inset);
      py = std::clamp(py, local.y * loc::kCellM + inset,
                      (local.y + 1) * loc::kCellM - inset);
    }
    const float cs = std::cos(ray_yaw), sn = std::sin(ray_yaw);
    // During a pivot, all three short rays can briefly measure parallel walls.
    // If measured history already places the robot in this cell, two reliable
    // same-axis returns may preserve that partial hypothesis while the third
    // return is grazing or endpoint-unstable. Do not let endpoint enumeration
    // manufacture the unobserved coordinate from that third ray.
    bool partial_same_axis = false, partial_reacquirable = false;
    int partial_axis = -1;
    float pinned_x = px, pinned_y = py;
    if (!startup && accepted_.timestamp_us && c == previous) {
      unsigned reliable_count[2]{};
      float coordinate_min[2] = {INFINITY, INFINITY};
      float coordinate_max[2] = {-INFINITY, -INFINITY};
      int unstable_count = 0;
      bool geometrically_unstable = false;
      for (int i = 0; i < 3; ++i) {
        const auto &mount = loc::cfg::lidar_mounts[i];
        const auto hit = loc::trace_maze_ray(map, px, py, ray_yaw, mount);
        if (hit.status != loc::RayStatus::HIT || hit.axis < 0 || hit.axis > 1)
          continue;
        const float component = hit.axis == 0
                                    ? std::cos(ray_yaw + mount.yaw_body_rad)
                                    : std::sin(ray_yaw + mount.yaw_body_rad);
        const float ox = cs * mount.x_body_m - sn * mount.y_body_m;
        const float oy = sn * mount.x_body_m + cs * mount.y_body_m;
        const float coordinate =
            hit.line_m - (hit.axis == 0 ? ox : oy) - component * f.range_m[i];
        const bool reliable =
            !hit.ambiguous && std::fabs(component) >= .17364818f &&
            std::fabs(f.range_m[i] - hit.range) <= range_tolerance;
        if (reliable) {
          ++reliable_count[hit.axis];
          coordinate_min[hit.axis] =
              std::min(coordinate_min[hit.axis], coordinate);
          coordinate_max[hit.axis] =
              std::max(coordinate_max[hit.axis], coordinate);
        } else {
          ++unstable_count;
          geometrically_unstable =
              hit.ambiguous || std::fabs(component) < .17364818f;
        }
      }
      for (int axis = 0; axis < 2; ++axis) {
        if (reliable_count[1 - axis] == 0 &&
            (reliable_count[axis] == 3 ||
             (reliable_count[axis] == 2 && unstable_count == 1 &&
              geometrically_unstable)) &&
            coordinate_max[axis] - coordinate_min[axis] <= cfg_.sigma_m) {
          partial_same_axis = true;
          partial_reacquirable =
              reliable_count[axis] == 2 && unstable_count == 1;
          partial_axis = axis;
          break;
        }
      }
    }
    auto complete_cost = [&](float x, float y, int x_ray, int y_ray) {
      float cost = 0;
      unsigned axes = 0, support[2]{}, normal_signs[2]{};
      for (int i = 0; i < 3; ++i) {
        const auto hit =
            loc::trace_maze_ray(map, x, y, ray_yaw, loc::cfg::lidar_mounts[i]);
        if (hit.status != loc::RayStatus::HIT ||
            (i == x_ray && hit.axis != 0) || (i == y_ray && hit.axis != 1))
          return std::numeric_limits<float>::infinity();
        const float component =
            hit.axis == 0
                ? std::cos(ray_yaw + loc::cfg::lidar_mounts[i].yaw_body_rad)
                : std::sin(ray_yaw + loc::cfg::lidar_mounts[i].yaw_body_rad);
        // A stale partial prior needs two opposing support walls and a
        // second normal. A near-normal endpoint ray may supply that normal
        // only when the independent support pair constrains its wall side;
        // grazing rays cannot manufacture the lost coordinate.
        if (partial_same_axis &&
            (std::fabs(component) < .17364818f ||
             (hit.ambiguous &&
              (hit.axis == partial_axis ||
               std::fabs(component) < std::cos(loc::deg2rad(10))))))
          return std::numeric_limits<float>::infinity();
        axes |= 1U << hit.axis;
        ++support[hit.axis];
        normal_signs[hit.axis] |= component > 0 ? 1U : 2U;
        const float residual = (f.range_m[i] - hit.range) / cfg_.sigma_m;
        if (std::fabs(residual) > 1)
          return std::numeric_limits<float>::infinity();
        cost += residual * residual;
      }
      if (partial_same_axis && (axes != 3 || support[partial_axis] != 2 ||
                                normal_signs[partial_axis] != 3))
        return std::numeric_limits<float>::infinity();
      return cost;
    };
    float complete_score = std::numeric_limits<float>::infinity();
    const bool reacquire_partial =
        partial_same_axis && partial_reacquirable &&
        oldest - accepted_.timestamp_us > cfg_.max_age_us &&
        std::fabs(f.imu.gz_rps) <= .05f;
    if (!startup && (!partial_same_axis || reacquire_partial)) {
      complete_score = complete_cost(px, py, -1, -1);
      {
        // A noisy prior can miss a finite wall and seed the wrong refit
        // basin. Enumerate measured X/Y wall-plane coordinates from different
        // channels, then verify every return with the physical segment tracer.
        // At most 18 coordinates per axis (3 rays x 6 grid lines), restricted
        // to this nearby candidate cell. No nominal motion seeds a pose.
        struct Coordinate {
          float value;
          int ray;
        };
        Coordinate xs[18]{}, ys[18]{};
        unsigned nx = 0, ny = 0;
        for (int i = 0; i < 3; ++i) {
          const auto &mount = loc::cfg::lidar_mounts[i];
          const float ox = cs * mount.x_body_m - sn * mount.y_body_m;
          const float oy = sn * mount.x_body_m + cs * mount.y_body_m;
          const float dx = std::cos(ray_yaw + mount.yaw_body_rad);
          const float dy = std::sin(ray_yaw + mount.yaw_body_rad);
          for (int line = 0; line <= loc::kMazeN; ++line) {
            const float x = line * loc::kCellM - ox - dx * f.range_m[i];
            const float y = line * loc::kCellM - oy - dy * f.range_m[i];
            constexpr float inset = 1e-5f;
            if (std::fabs(dx) >= .17364818f &&
                x >= local.x * loc::kCellM - cfg_.sigma_m &&
                x <= (local.x + 1) * loc::kCellM + cfg_.sigma_m)
              xs[nx++] = {std::clamp(x, local.x * loc::kCellM,
                                     (local.x + 1) * loc::kCellM - inset),
                          i};
            if (std::fabs(dy) >= .17364818f &&
                y >= local.y * loc::kCellM - cfg_.sigma_m &&
                y <= (local.y + 1) * loc::kCellM + cfg_.sigma_m)
              ys[ny++] = {std::clamp(y, local.y * loc::kCellM,
                                     (local.y + 1) * loc::kCellM - inset),
                          i};
          }
        }
        const float dt =
            std::min(.5f, float(oldest - accepted_.timestamp_us) * 1e-6f);
        auto consider = [&](float x, float y, int x_ray, int y_ray) {
          if (x < local.x * loc::kCellM || x >= (local.x + 1) * loc::kCellM ||
              y < local.y * loc::kCellM || y >= (local.y + 1) * loc::kCellM ||
              std::hypot(x - accepted_.x_m, y - accepted_.y_m) >
                  cfg_.max_translation_m_s * dt + .025f)
            return std::numeric_limits<float>::infinity();
          const float cost = complete_cost(x, y, x_ray, y_ray);
          if (cost < complete_score) {
            complete_score = cost;
            px = x;
            py = y;
          }
          return cost;
        };
        for (unsigned ix = 0; ix < nx; ++ix)
          for (unsigned iy = 0; iy < ny; ++iy) {
            if (xs[ix].ray == ys[iy].ray)
              continue;
            const float x = xs[ix].value, y = ys[iy].value;
            if (std::isfinite(consider(x, y, xs[ix].ray, ys[iy].ray)))
              continue;
            // Noisy plane coordinates may straddle an occlusion boundary.
            // Seed both sides of a nearby finite endpoint's beam line, then
            // demand a complete three-return fit at the new measured pose.
            // Endpoint proximity alone never discards a channel.
            for (int i = 0; i < 3; ++i) {
              const auto &mount = loc::cfg::lidar_mounts[i];
              const float ox = cs * mount.x_body_m - sn * mount.y_body_m;
              const float oy = sn * mount.x_body_m + cs * mount.y_body_m;
              const float dx = std::cos(ray_yaw + mount.yaw_body_rad);
              const float dy = std::sin(ray_yaw + mount.yaw_body_rad);
              const float ex = x + ox + dx * f.range_m[i];
              const float ey = y + oy + dy * f.range_m[i];
              const int gx = int(std::lround(ex / loc::kCellM));
              const int gy = int(std::lround(ey / loc::kCellM));
              if (gx < 0 || gx > loc::kMazeN || gy < 0 || gy > loc::kMazeN ||
                  std::hypot(ex - gx * loc::kCellM, ey - gy * loc::kCellM) >
                      cfg_.sigma_m ||
                  !finite_wall_endpoint(map, gx, gy))
                continue;
              const float normal_offset = -dy * (gx * loc::kCellM - x - ox) +
                                          dx * (gy * loc::kCellM - y - oy);
              for (float epsilon : {-1e-4f, 1e-4f})
                consider(x - dy * (normal_offset + epsilon),
                         y + dx * (normal_offset + epsilon), -1, -1);
            }
          }
      }
    }
    const bool complete_fit = !startup && complete_score <= cfg_.max_score;
    if (complete_fit)
      partial_same_axis = false;
    bool initial_match = true;
    int initial_wall[3]{};
    unsigned count[2]{};
    bool corroborated[2]{}, single_endpoint[2]{};
    unsigned fit_mask = 0;
    // A side ray near an opening can be endpoint-ambiguous while the front
    // and opposite side still independently observe both coordinates. Refit
    // runtime observations without that unstable return instead of rejecting
    // the entire pose and deadlocking a controller that pauses on pose loss.
    // Startup retains the strict three-ray/permutation correspondence rules.
    for (unsigned pass = 0; pass < (startup ? 1U : 3U); ++pass) {
      const float previous_x = px, previous_y = py;
      if (excluded_rays) {
        unsigned support_axes = 0;
        unsigned support_count[2]{};
        float support_min[2] = {INFINITY, INFINITY};
        float support_max[2] = {-INFINITY, -INFINITY};
        for (int i = 0; i < 3; ++i) {
          if (excluded_rays & (1U << i))
            continue;
          const auto support = loc::trace_maze_ray(map, px, py, ray_yaw,
                                                   loc::cfg::lidar_mounts[i]);
          if (support.status == loc::RayStatus::HIT && !support.ambiguous &&
              support.axis >= 0 && support.axis < 2)
            support_axes |= 1U << support.axis;
          if (support.status == loc::RayStatus::HIT && !support.ambiguous &&
              support.axis >= 0 && support.axis < 2 &&
              std::fabs(f.range_m[i] - support.range) <= range_tolerance) {
            const auto &mount = loc::cfg::lidar_mounts[i];
            const float component =
                support.axis == 0 ? std::cos(ray_yaw + mount.yaw_body_rad)
                                  : std::sin(ray_yaw + mount.yaw_body_rad);
            const float ox = cs * mount.x_body_m - sn * mount.y_body_m;
            const float oy = sn * mount.x_body_m + cs * mount.y_body_m;
            const float coordinate = support.line_m -
                                     (support.axis == 0 ? ox : oy) -
                                     component * f.range_m[i];
            ++support_count[support.axis];
            support_min[support.axis] =
                std::min(support_min[support.axis], coordinate);
            support_max[support.axis] =
                std::max(support_max[support.axis], coordinate);
          }
        }
        // Refitting may expose a different endpoint configuration. Restore
        // all beams if the pair supporting an exclusion loses observability.
        const bool partial_support =
            partial_same_axis && support_count[partial_axis] == 2 &&
            support_max[partial_axis] - support_min[partial_axis] <=
                cfg_.sigma_m;
        if (support_axes != 3 && !partial_support)
          excluded_rays = 0;
      }
      float sum[2]{}, endpoint_sum[2]{};
      float endpoint_min[2] = {INFINITY, INFINITY},
            endpoint_max[2] = {-INFINITY, -INFINITY};
      unsigned endpoint_count[2]{}, endpoint_signs[2]{}, endpoint_mask[2]{};
      int first_wall[2] = {-1, -1};
      bool different_walls[2]{};
      float reliable_min[2] = {INFINITY, INFINITY},
            reliable_max[2] = {-INFINITY, -INFINITY};
      float endpoint_alignment[2] = {1, 1};
      count[0] = count[1] = 0;
      corroborated[0] = corroborated[1] = false;
      single_endpoint[0] = single_endpoint[1] = false;
      fit_mask = 0;
      for (int i = 0; i < 3; ++i) {
        if (excluded_rays & (1U << i))
          continue;
        const auto &mount = loc::cfg::lidar_mounts[i];
        const auto hit = loc::trace_maze_ray(map, px, py, ray_yaw, mount);
        if (hit.status != loc::RayStatus::HIT || hit.axis < 0 || hit.axis > 1)
          return;
        const float component = hit.axis == 0
                                    ? std::cos(ray_yaw + mount.yaw_body_rad)
                                    : std::sin(ray_yaw + mount.yaw_body_rad);
        // A grazing beam can keep the same wall ID while a few millimeters
        // of pose uncertainty move its intersection far along that wall.
        // Treat incidence within 10 degrees of tangent as association-unstable
        // too; exclusion still requires two other reliable rays measuring XY.
        constexpr float minimum_normal_component = .17364818f; // sin(10 deg)
        if (!startup &&
            (hit.ambiguous || std::fabs(component) < minimum_normal_component ||
             (pass && hit.wall_id != initial_wall[i])))
          endpoint_seen |= 1U << i;
        initial_wall[i] = hit.wall_id;
        if (!startup && !complete_fit && (endpoint_seen & (1U << i)) &&
            std::fabs(f.range_m[i] - hit.range) > cfg_.sigma_m) {
          unsigned independent_axes = 0;
          for (int other = 0; other < 3; ++other) {
            if (other == i)
              continue;
            const auto support = loc::trace_maze_ray(
                map, px, py, ray_yaw, loc::cfg::lidar_mounts[other]);
            if (support.status == loc::RayStatus::HIT && !support.ambiguous &&
                support.axis >= 0 && support.axis < 2 &&
                std::fabs(f.range_m[other] - support.range) <= range_tolerance)
              independent_axes |= 1U << support.axis;
          }
          unsigned same_axis_supports = 0;
          float same_axis_min = INFINITY, same_axis_max = -INFINITY;
          if (partial_same_axis) {
            for (int other = 0; other < 3; ++other) {
              if (other == i)
                continue;
              const auto support = loc::trace_maze_ray(
                  map, px, py, ray_yaw, loc::cfg::lidar_mounts[other]);
              if (support.status != loc::RayStatus::HIT || support.ambiguous ||
                  support.axis != partial_axis ||
                  std::fabs(f.range_m[other] - support.range) > range_tolerance)
                continue;
              const auto &support_mount = loc::cfg::lidar_mounts[other];
              const float support_component =
                  partial_axis == 0
                      ? std::cos(ray_yaw + support_mount.yaw_body_rad)
                      : std::sin(ray_yaw + support_mount.yaw_body_rad);
              const float support_ox =
                  cs * support_mount.x_body_m - sn * support_mount.y_body_m;
              const float support_oy =
                  sn * support_mount.x_body_m + cs * support_mount.y_body_m;
              const float support_coordinate =
                  support.line_m -
                  (partial_axis == 0 ? support_ox : support_oy) -
                  support_component * f.range_m[other];
              ++same_axis_supports;
              same_axis_min = std::min(same_axis_min, support_coordinate);
              same_axis_max = std::max(same_axis_max, support_coordinate);
            }
          }
          if (independent_axes == 3 ||
              (partial_same_axis && same_axis_supports == 2 &&
               same_axis_max - same_axis_min <= cfg_.sigma_m)) {
            // An endpoint can switch to an unrelated, apparently unambiguous
            // wall after a refit. Do not admit that switched association on a
            // later pass. Both coordinates must instead be freshly measured
            // by the other two reliable beams. Retain this channel uncertainty
            // across frames until its mapped return agrees again.
            excluded_rays |= 1U << i;
            continue;
          }
        }
        if (excluded_rays & (1U << i))
          continue;
        const float ox = cs * mount.x_body_m - sn * mount.y_body_m;
        const float oy = sn * mount.x_body_m + cs * mount.y_body_m;
        // Compare each initial distance with the reference wall, allowing
        // any position inside this cell plus the configured range noise.
        if (startup &&
            std::fabs((f.range_m[i] - hit.range) * component) >
                .5f * loc::kCellM + range_tolerance * std::fabs(component))
          initial_match = false;
        const float coordinate =
            hit.line_m - (hit.axis == 0 ? ox : oy) - component * f.range_m[i];
        if (!startup && hit.ambiguous) {
          endpoint_sum[hit.axis] += coordinate;
          endpoint_min[hit.axis] = std::min(endpoint_min[hit.axis], coordinate);
          endpoint_max[hit.axis] = std::max(endpoint_max[hit.axis], coordinate);
          ++endpoint_count[hit.axis];
          endpoint_signs[hit.axis] |= component > 0 ? 1U : 2U;
          endpoint_mask[hit.axis] |= 1U << i;
          endpoint_alignment[hit.axis] =
              std::min(endpoint_alignment[hit.axis], std::fabs(component));
          continue;
        }
        sum[hit.axis] += coordinate;
        reliable_min[hit.axis] = std::min(reliable_min[hit.axis], coordinate);
        reliable_max[hit.axis] = std::max(reliable_max[hit.axis], coordinate);
        if (first_wall[hit.axis] >= 0 && first_wall[hit.axis] != hit.wall_id)
          different_walls[hit.axis] = true;
        first_wall[hit.axis] = hit.wall_id;
        ++count[hit.axis];
        fit_mask |= 1U << i;
      }
      for (unsigned axis = 0; axis < 2 && !startup; ++axis) {
        // At a corridor boundary both side beams can approach segment ends.
        // They may constrain the missing axis together, but only if opposing
        // independent returns agree and a reliable ray observes the other axis.
        if (partial_same_axis && axis != partial_axis) {
          // Keep this coordinate at measured history; parallel rays do not
          // observe it, and endpoint inference is specifically disallowed.
        } else if (!count[axis] && count[1 - axis] &&
                   endpoint_count[axis] >= 2 && endpoint_signs[axis] == 3 &&
                   endpoint_max[axis] - endpoint_min[axis] <= cfg_.sigma_m) {
          sum[axis] = endpoint_sum[axis];
          count[axis] = endpoint_count[axis];
          fit_mask |= endpoint_mask[axis];
          corroborated[axis] = true;
        } else if (!count[axis] && endpoint_count[axis] == 1 &&
                   count[1 - axis] >= 2 && different_walls[1 - axis] &&
                   reliable_max[1 - axis] - reliable_min[1 - axis] <=
                       cfg_.sigma_m &&
                   endpoint_alignment[axis] >= std::cos(loc::deg2rad(10))) {
          // A slightly tilted corridor frame can put one side ray near an
          // endpoint while the front and opposite side hit two distinct walls
          // on the other axis. Their agreeing coordinates constrain which side
          // of that endpoint we occupy. Keep the actual near-normal endpoint
          // measurement, and require every wall identity to survive refitting.
          sum[axis] = endpoint_sum[axis];
          count[axis] = 1;
          fit_mask |= endpoint_mask[axis];
          corroborated[axis] = single_endpoint[axis] = true;
        }
      }
      // An unobservable candidate still competes: deleting it can make
      // an aliased variant falsely unique at an arbitrary initial yaw.
      if (count[0])
        px = sum[0] / count[0];
      if (count[1])
        py = sum[1] / count[1];
      if (partial_same_axis) {
        if (partial_axis == 0)
          py = pinned_y;
        else
          px = pinned_x;
      }
      if (!startup && constrain_to_cell) {
        // Range noise at a shared cell edge can put the unconstrained fit a
        // fraction beyond that edge and select a completely different finite
        // wall on the next pass. Fit within this candidate's box; large
        // corrections still reject it, and all final ray residual, visibility,
        // observability and measured-history checks remain mandatory.
        constexpr float inset = 1e-5f;
        const float bounded_x = std::clamp(px, local.x * loc::kCellM + inset,
                                           (local.x + 1) * loc::kCellM - inset);
        const float bounded_y = std::clamp(py, local.y * loc::kCellM + inset,
                                           (local.y + 1) * loc::kCellM - inset);
        if (std::fabs(bounded_x - px) > cfg_.sigma_m ||
            std::fabs(bounded_y - py) > cfg_.sigma_m)
          return;
        px = bounded_x;
        py = bounded_y;
      }
      if (complete_fit) {
        const bool inside =
            px >= local.x * loc::kCellM && px < (local.x + 1) * loc::kCellM &&
            py >= local.y * loc::kCellM && py < (local.y + 1) * loc::kCellM;
        const float cost = inside ? complete_cost(px, py, -1, -1)
                                  : std::numeric_limits<float>::infinity();
        if (cost > complete_score + 1e-6f) {
          px = previous_x;
          py = previous_y;
          break; // Preserve a complete fit across discontinuous wall choices.
        }
        complete_score = cost;
      }
    }
    // A quarter turn can swap a cell's inclusive and exclusive edges. During
    // startup allow both reference edges, then apply the world-cell bounds.
    if (px < local.x * loc::kCellM || py < local.y * loc::kCellM ||
        (startup ? px > (local.x + 1) * loc::kCellM
                 : px >= (local.x + 1) * loc::kCellM) ||
        (startup ? py > (local.y + 1) * loc::kCellM
                 : py >= (local.y + 1) * loc::kCellM))
      return;
    float score = 0;
    bool trusted = initial_match && count[0] && count[1] && !partial_same_axis;
    unsigned observed[2]{}, signs[2]{}, scored = 0;
    int observed_wall[2] = {-1, -1};
    bool distinct_observed_walls[2]{};
    float observed_min[2] = {INFINITY, INFINITY},
          observed_max[2] = {-INFINITY, -INFINITY};
    for (int i = 0; i < 3; ++i) {
      if (excluded_rays & (1U << i))
        continue;
      if (partial_same_axis) {
        const auto partial_hit = loc::trace_maze_ray(map, px, py, ray_yaw,
                                                     loc::cfg::lidar_mounts[i]);
        const float component =
            partial_hit.axis == 0
                ? std::cos(ray_yaw + loc::cfg::lidar_mounts[i].yaw_body_rad)
                : std::sin(ray_yaw + loc::cfg::lidar_mounts[i].yaw_body_rad);
        if (partial_hit.status == loc::RayStatus::HIT &&
            (partial_hit.ambiguous || partial_hit.axis != partial_axis ||
             std::fabs(component) < .17364818f))
          continue;
      }
      const auto hit =
          loc::trace_maze_ray(map, px, py, ray_yaw, loc::cfg::lidar_mounts[i]);
      if (hit.status != loc::RayStatus::HIT)
        return;
      if (!startup && hit.ambiguous &&
          (!corroborated[hit.axis] || !(fit_mask & (1U << i)) ||
           hit.wall_id != initial_wall[i]))
        continue;
      if (!startup && corroborated[hit.axis] &&
          (!(fit_mask & (1U << i)) || hit.wall_id != initial_wall[i]))
        trusted = false;
      if (!startup && (single_endpoint[0] || single_endpoint[1]) &&
          hit.wall_id != initial_wall[i])
        trusted = false;
      // A fit that changes the initially matched wall is not sufficient
      // to identify a startup permutation. Retain it as a competitor.
      if (startup && (hit.ambiguous || hit.wall_id != initial_wall[i]))
        trusted = false;
      const float residual = (f.range_m[i] - hit.range) / cfg_.sigma_m;
      score += residual * residual;
      const auto &mount = loc::cfg::lidar_mounts[i];
      const float component = hit.axis == 0
                                  ? std::cos(ray_yaw + mount.yaw_body_rad)
                                  : std::sin(ray_yaw + mount.yaw_body_rad);
      const float origin = hit.axis == 0
                               ? cs * mount.x_body_m - sn * mount.y_body_m
                               : sn * mount.x_body_m + cs * mount.y_body_m;
      const float coordinate = hit.line_m - origin - component * f.range_m[i];
      observed_min[hit.axis] = std::min(observed_min[hit.axis], coordinate);
      observed_max[hit.axis] = std::max(observed_max[hit.axis], coordinate);
      signs[hit.axis] |= component > 0 ? 1U : 2U;
      ++observed[hit.axis];
      if (observed_wall[hit.axis] >= 0 &&
          observed_wall[hit.axis] != hit.wall_id)
        distinct_observed_walls[hit.axis] = true;
      observed_wall[hit.axis] = hit.wall_id;
      ++scored;
    }
    if (!startup) {
      trusted = trusted && observed[0] && observed[1];
      for (unsigned axis = 0; axis < 2; ++axis)
        if (corroborated[axis]) {
          if (single_endpoint[axis]) {
            if (observed[axis] != 1 || observed[1 - axis] < 2 ||
                !distinct_observed_walls[1 - axis] ||
                observed_max[1 - axis] - observed_min[1 - axis] > cfg_.sigma_m)
              trusted = false;
          } else if (observed[axis] < 2 || signs[axis] != 3 ||
                     observed_max[axis] - observed_min[axis] > cfg_.sigma_m)
            trusted = false;
        }
      if (scored)
        score *= 3.f / scored;
    }
    if (score > cfg_.max_score)
      return;
    // All history, output cells and positions use the world frame.
    for (unsigned i = 0; i < turns; ++i) {
      const float next_x = loc::kMazeN * loc::kCellM - py;
      py = px;
      px = next_x;
    }
    if (px < c.x * loc::kCellM || px >= (c.x + 1) * loc::kCellM ||
        py < c.y * loc::kCellM || py >= (c.y + 1) * loc::kCellM)
      return;
    // A distant ray alias must not replace measured history while putting
    // the chassis through a wall. Local fits retain their noise tolerance:
    // this is a reacquisition sanity check, not a motion-clearance proof.
    // The turn guard still requires its complete padded footprint sweep.
    const bool distant_refit =
        accepted_.timestamp_us &&
        std::hypot(px - accepted_.x_m, py - accepted_.y_m) > .05f;
    if (!startup && distant_refit &&
        !mapped_footprint_clear(maps_.variants[v], px, py, yaw_, 0.f))
      return;
    if (accepted_.timestamp_us) {
      const float dt = float(oldest - accepted_.timestamp_us) * 1e-6f;
      if (dt <= 0 || std::hypot(px - accepted_.x_m, py - accepted_.y_m) >
                         cfg_.max_translation_m_s * std::min(dt, .5f) + .025f)
        return;
    }
    // Cross-reference this feature with the sequence of previous
    // map-cell hypotheses, including those not yet uniquely resolved.
    float prior = 0;
    if (have_hypotheses_) {
      prior = std::numeric_limits<float>::infinity();
      for (int oy = -1; oy <= 1; ++oy)
        for (int ox = -1; ox <= 1; ++ox) {
          if (std::abs(ox) + std::abs(oy) > 1)
            continue;
          const loc::Cell previous{c.x + ox, c.y + oy};
          if (!loc::valid_cell(previous) ||
              (previous != c && !maps_.variants[v].can_step(previous, c)))
            continue;
          const auto &h = hypotheses_[v * 25 + previous.y * 5 + previous.x];
          if (!h.timestamp || oldest <= h.timestamp)
            continue;
          // An untrusted fit cannot move the history anchor along an
          // unobserved axis. Reacquire against the last measured full pose.
          const bool measured_anchor = accepted_.timestamp_us && !h.trusted;
          const float hx = measured_anchor ? accepted_.x_m : h.x;
          const float hy = measured_anchor ? accepted_.y_m : h.y;
          const int64_t epoch =
              measured_anchor ? accepted_.timestamp_us : h.timestamp;
          const float dt = std::min(.5f, float(oldest - epoch) * 1e-6f);
          if (std::hypot(px - hx, py - hy) <=
              cfg_.max_translation_m_s * dt + .025f)
            prior = std::min(prior, .8f * h.cost);
        }
      if (!std::isfinite(prior))
        return;
    }
    score += prior;
    const int candidate_key = int(v) * 25 + c.y * 5 + c.x;
    next_[candidate_key] = {px,      py,    score, uint8_t(excluded_rays),
                            trusted, oldest};
    out_.candidate_cells[v] |= uint32_t{1} << (c.y * 5 + c.x);
    ++out_.candidates;
    if (score < best) {
      second = best;
      best = score;
      bx = px;
      by = py;
      cell = c;
      variant = v;
      key = candidate_key;
      best_trusted = trusted;
    } else
      second = std::min(second, score);
  };
  if (kind_ == loc::MapKind::UNKNOWN) {
    constexpr float quarter = loc::kPi / 2;
    const unsigned heading =
        unsigned((int(std::lround(yaw_ / quarter)) + 4) % 4);
    const float offset = loc::wrap_pi(yaw_ - heading * quarter);
    // Four cyclic L/F/R orientations in the reference Q1 frame. Keep the
    // measured deviation from cardinal; arbitrary initial yaw is supported.
    for (unsigned permutation = 0; permutation < 4; ++permutation) {
      const float local_yaw = permutation * quarter + offset;
      // MAG relates the matched local heading to a world map rotation:
      // rotation = world heading - reference heading (modulo four).
      const unsigned turns = (heading + 4 - permutation) % 4;
      for (unsigned v = 0; v < 4; ++v) {
        if (!(cfg_.variant_mask & (1U << v)) || maps_.turns[v] % 4 != turns)
          continue;
        // Defaults reduce to the compiled Q1 table. Inverse rotation also
        // honors independently configured variant geometry and turn labels.
        const auto reference =
            maps_.variants[v].rotated((4 - turns) % 4, loc::MapKind::Q1);
        // Physical starts are the configured Q1-frame entrance cells, with
        // arbitrary facing. Legacy color/heading masks do not constrain the
        // FEATURE wall-signature handshake. Runtime tracking below still
        // searches the complete map after the first accepted entrance pose.
        for (unsigned start = 0; start < maps_.start_count; ++start) {
          bool duplicate = false;
          for (unsigned prior = 0; prior < start; ++prior)
            duplicate = duplicate ||
                        maps_.starts[prior].cell == maps_.starts[start].cell;
          if (!duplicate)
            evaluate(v, reference, maps_.starts[start].cell, local_yaw, turns,
                     true, false);
        }
      }
    }
  } else {
    const unsigned v = unsigned(kind_);
    for (int y = 0; y < loc::kMazeN; ++y)
      for (int x = 0; x < loc::kMazeN; ++x)
        evaluate(v, maps_.variants[v], {x, y}, yaw_, 0, false, false);
    const bool bounded_recovery = key < 0;
    if (bounded_recovery) {
      // Preserve the normal candidate/ambiguity search. Only a frame with no
      // geometrically valid fit retries the box-constrained measurement fit;
      // do not introduce boundary-clamped aliases beside an existing fit.
      for (int y = 0; y < loc::kMazeN; ++y)
        for (int x = 0; x < loc::kMazeN; ++x)
          evaluate(v, maps_.variants[v], {x, y}, yaw_, 0, false, true);
    }
    if (key >= 0 && best_trusted) {
      // Adjacent cells on one known-open corridor can describe the same
      // continuous measured pose near their shared boundary. Their small
      // box-constrained fit difference is not a competing map location.
      // Keep every candidate in diagnostics and history; only the runner-up
      // margin groups these independently trusted, nearby measurements.
      second = std::numeric_limits<float>::infinity();
      for (int other = 0; other < 25; ++other) {
        const auto &h = next_[v * 25 + other];
        if (!h.timestamp || int(v) * 25 + other == key)
          continue;
        const loc::Cell neighbor{other % 5, other / 5};
        if (h.trusted && maps_.variants[v].can_step(cell, neighbor) &&
            std::hypot(h.x - bx, h.y - by) <= cfg_.sigma_m)
          continue;
        second = std::min(second, h.cost);
      }
    }
  }
  out_.resolution.score = best;
  out_.resolution.margin = std::isfinite(best) ? second - best : 0;
  if (key < 0) {
    // Startup uncertainty is not a contradiction. Only distinct, fresh,
    // heading-qualified frames reach here, after rejecting skew/pivot frames.
    if (accepted_.timestamp_us && !out_.contradiction_latched) {
      if (!last_contradiction_us_ ||
          oldest - last_contradiction_us_ > cfg_.max_age_us)
        out_.contradiction_frames = 0;
      last_contradiction_us_ = oldest;
      out_.contradiction_frames =
          std::min(out_.contradiction_frames + 1, cfg_.contradiction_limit);
      if (out_.contradiction_frames == cfg_.contradiction_limit &&
          !out_.contradiction_latched) {
        out_.contradiction_latched = true;
        out_.contradiction_us = oldest;
      }
    }
    return fail(ResolveStatus::NO_CANDIDATE);
  }
  if (!out_.contradiction_latched)
    out_.contradiction_frames = 0;
  last_contradiction_us_ = 0;
  hypotheses_ = next_;
  have_hypotheses_ = true;
  // Endpoint-ambiguous matches remain competitors; dropping them could
  // falsely make a geometrically repeated distant feature look unique.
  if (!best_trusted || second - best < cfg_.min_margin)
    return fail(ResolveStatus::AMBIGUOUS);
  if (key != candidate_) {
    candidate_ = key;
    // A measured pose crossing an open cell boundary is continuous evidence,
    // including a sub-millimeter box-constrained fit on either side. Preserve
    // an established runtime confirmation when the fresh, fully observable
    // pose stays within one sigma; startup and distinct locations still need
    // their independent confirmation sequence.
    const bool continuous =
        kind_ != loc::MapKind::UNKNOWN && accepted_.timestamp_us > 0 &&
        oldest > accepted_.timestamp_us &&
        oldest - accepted_.timestamp_us <= cfg_.max_age_us &&
        confirmations_ >= cfg_.confirmations &&
        maps_.variants[variant].can_step({accepted_.grid_x, accepted_.grid_y},
                                         cell) &&
        std::hypot(bx - accepted_.x_m, by - accepted_.y_m) <= cfg_.sigma_m;
    confirmations_ = continuous ? cfg_.confirmations : 1;
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
  out_.excluded_ray_mask = next_[key].excluded_rays;
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
