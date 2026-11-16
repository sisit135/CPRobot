// Withdrawn V2 TurnGuard front-profile extension (2026-11-16).
// All changes below are commented out; this header contains no active code.
// Active sources were restored to the state immediately after the triangular
// front geometry update. The triangular head and non-stop turns are retained.
// In this unified diff, minus lines are restored code; plus lines are the
// disabled extension. Preserve for review; do not enable without revalidation.
// See docs/validation/v2_turnguard_front_profile_2026-11-16.md.
//
// --- a/fw/robot_sf/include/navigation/turn_guard.hpp
// +++ b/fw/robot_sf/include/navigation/turn_guard.hpp
// @@ -75,6 +75,7 @@
//    TurnGuardPhase phase() const { return phase_; }
//    float left_clearance_m() const { return left_; }
//    float right_clearance_m() const { return right_; }
// +  float front_clearance_m() const { return front_; }
//    void cancel() { *this = TurnGuard{}; }
//    DriveRequest update(const loc::MazeMap &map,
//                        const loc::PoseSnapshot &observed,
// @@ -133,7 +134,14 @@
//        p.x_m = filtered_x_ - a * std::cos(p.yaw_rad);
//        p.y_m = filtered_y_ - a * std::sin(p.yaw_rad);
//      }
// +    const bool rolling_turn = requested.linear_m_s != 0 &&
// +                              requested.angular_rad_s != 0 &&
// +                              (requested.status == ExecuteStatus::TURN ||
// +                               (requested.status == ExecuteStatus::FALLBACK &&
// +                                requested.diagnostics.pivot_angle_valid));
// +    const bool unguarded_rolling = rolling_turn && !busy() && !permitted_;
//      const bool pivot =
// +        rolling_turn ||
//          (requested.linear_m_s == 0 && requested.angular_rad_s != 0) ||
//          requested.diagnostics.decision == ExecuteDecision::WAIT_TURN_CLEARANCE;
//      if (!busy() && !pivot) {
// @@ -164,7 +172,7 @@
//      DriveRequest out = requested;
//      out.linear_m_s = out.angular_rad_s = 0;
//      out.status = ExecuteStatus::PAUSED;
// -    if (!busy() && !continuing) {
// +    if (!busy() && !continuing && !unguarded_rolling) {
//        phase_ = TurnGuardPhase::COAST;
//        started_ = now;
//        quiet_since_ = last_pose_ = 0;
// @@ -180,7 +188,7 @@
//        out.status = ExecuteStatus::BLOCKED;
//        return out;
//      }
// -    if ((continuing && heading &&
// +    if (((continuing || unguarded_rolling) && heading &&
//           (!heading->valid || !fresh(heading->timestamp_us, now, 100000) ||
//            !std::isfinite(heading->yaw_rad))) ||
//          !fresh(p.timestamp_us, now, 150000) || !p.tracking || !map.valid() ||
// @@ -201,6 +209,14 @@
//          stable_ = 0;
//          return out;
//        }
// +    if (!std::isfinite(requested.linear_m_s) ||
// +        !std::isfinite(requested.angular_rad_s) ||
// +        (requested.diagnostics.pivot_angle_valid &&
// +         (!std::isfinite(requested.diagnostics.pivot_angle_rad) ||
// +          std::fabs(requested.diagnostics.pivot_angle_rad) > 2 * loc::kPi))) {
// +      stable_ = 0;
// +      return out;
// +    }
//      const bool distinct = p.timestamp_us > last_pose_;
//      const bool stable =
//          last_pose_ && std::hypot(p.x_m - last_x_, p.y_m - last_y_) <= .005f;
// @@ -224,6 +240,8 @@
//      const float cardinal =
//          std::round(p.yaw_rad / (loc::kPi / 2)) * (loc::kPi / 2);
//      const float error = loc::wrap_pi(p.yaw_rad - cardinal);
// +    if (unguarded_rolling)
// +      pivot_target_valid_ = false;
//      if (!pivot_target_valid_) {
//        const float angle =
//            requested.diagnostics.pivot_angle_valid
// @@ -237,7 +255,35 @@
//          cardinal_wall_clearance(f.range_m[0], loc::cfg::lidar_mounts[0], error);
//      right_ =
//          cardinal_wall_clearance(f.range_m[2], loc::cfg::lidar_mounts[2], error);
// +    front_ =
// +        cardinal_wall_clearance(f.range_m[1], loc::cfg::lidar_mounts[1], error);
// +    if (unguarded_rolling) {
// +      const float radius = requested.linear_m_s / requested.angular_rad_s;
// +      const float angle = remaining_angle(p.yaw_rad);
// +      if (angle * requested.angular_rad_s > 0 &&
// +          mapped_rolling_turn_clear(map, p.x_m, p.y_m, p.yaw_rad, angle,
// +                                    radius) &&
// +          ranges_clear(map, p, f, radius)) {
// +        // A clear rolling turn needs no stationary dwell or latched axle.
// +        // Reprove its current curvature and front profile on every frame.
// +        pivot_target_valid_ = false;
// +        return requested;
// +      }
// +      // Hold the requested turn while the normal bounded recovery chooses
// +      // a posture for a stationary rear-axle pivot.
// +      phase_ = TurnGuardPhase::COAST;
// +      started_ = now;
// +      quiet_since_ = last_pose_ = 0;
// +      stable_ = 0;
// +      out.diagnostics.decision = ExecuteDecision::WAIT_TURN_CLEARANCE;
// +      return out;
// +    }
//      if (continuing) {
// +      // A noisy/range-only rejection stops this frame but preserves the
// +      // already proved stationary axle and fresh-heading continuation. It
// +      // must not demand translation reacquisition in mid-pivot.
// +      if (!ranges_clear(map, p, f))
// +        return out;
//        if (!mapped_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
//                                     remaining_angle(p.yaw_rad))) {
//          // Stop immediately, then re-observe at rest within the original
// @@ -261,9 +307,9 @@
//        return out;
//      }
//      if (phase_ == TurnGuardPhase::COAST && settled) {
// -      if (mapped_observable_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
// -                                             remaining_angle(p.yaw_rad)) &&
// -          ranges_clear(p, f, cardinal)) {
// +      if (pivot_exit_clear(map, p.x_m, p.y_m, p.yaw_rad,
// +                           remaining_angle(p.yaw_rad)) &&
// +          ranges_clear(map, p, f)) {
//          phase_ = TurnGuardPhase::IDLE;
//          permitted_ = true;
//          pivot_axle_x_ =
// @@ -272,8 +318,8 @@
//              p.y_m + motor::cfg::wheel_mount_x_body_m * std::sin(p.yaw_rad);
//          return authorized_request(requested, p.yaw_rad);
//        }
// -      if (mapped_observable_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
// -                                             remaining_angle(p.yaw_rad))) {
// +      if (pivot_exit_clear(map, p.x_m, p.y_m, p.yaw_rad,
// +                           remaining_angle(p.yaw_rad))) {
//          // Geometry is sufficient: wait for independent fresh range proof.
//          return out;
//        }
// @@ -300,9 +346,9 @@
//      if (phase_ == TurnGuardPhase::VERIFY && settled) {
//        if (straight_ && quiet_samples_ < 20)
//          return out;
// -      if (ranges_clear(p, f, base_) &&
// -          mapped_observable_axle_pivot_clear(map, p.x_m, p.y_m, p.yaw_rad,
// -                                             remaining_angle(p.yaw_rad)) &&
// +      if (ranges_clear(map, p, f) &&
// +          pivot_exit_clear(map, p.x_m, p.y_m, p.yaw_rad,
// +                           remaining_angle(p.yaw_rad)) &&
//            std::fabs(loc::wrap_pi(base_ - p.yaw_rad)) <= loc::deg2rad(2)) {
//          phase_ = TurnGuardPhase::IDLE;
//          permitted_ = true;
// @@ -370,12 +416,19 @@
//          std::copysign(out.angular_rad_s == 0 ? 0 : .04f, out.angular_rad_s);
//      const float axle = motor::cfg::wheel_mount_x_body_m;
//      if ((out.linear_m_s != 0 || out.angular_rad_s != 0) &&
// -        !sweep(map, p.x_m, p.y_m, p.yaw_rad,
// -               p.x_m + look * std::cos(p.yaw_rad) +
// -                   axle * (std::cos(p.yaw_rad) - std::cos(look_yaw)),
// -               p.y_m + look * std::sin(p.yaw_rad) +
// -                   axle * (std::sin(p.yaw_rad) - std::sin(look_yaw)),
// -               look_yaw)) {
// +        (!sweep(map, p.x_m, p.y_m, p.yaw_rad,
// +                p.x_m + look * std::cos(p.yaw_rad) +
// +                    axle * (std::cos(p.yaw_rad) - std::cos(look_yaw)),
// +                p.y_m + look * std::sin(p.yaw_rad) +
// +                    axle * (std::sin(p.yaw_rad) - std::sin(look_yaw)),
// +                look_yaw) ||
// +         !recovery_ranges_clear(
// +             map, p, f,
// +             p.x_m + look * std::cos(p.yaw_rad) +
// +                 axle * (std::cos(p.yaw_rad) - std::cos(look_yaw)),
// +             p.y_m + look * std::sin(p.yaw_rad) +
// +                 axle * (std::sin(p.yaw_rad) - std::sin(look_yaw)),
// +             look_yaw))) {
//        phase_ = TurnGuardPhase::BLOCKED;
//        out.linear_m_s = out.angular_rad_s = 0;
//        out.status = ExecuteStatus::BLOCKED;
// @@ -387,7 +440,7 @@
//  private:
//    TurnGuardPhase phase_ = TurnGuardPhase::IDLE;
//    bool permitted_ = false;
// -  float left_ = 0, right_ = 0;
// +  float left_ = 0, right_ = 0, front_ = 0;
//  #if NAV_TURN_GUARD
//    unsigned quiet_samples_ = 0;
//    int64_t filter_us_ = 0, last_update_us_ = 0;
// @@ -410,6 +463,9 @@
//      return angle;
//    }
//    DriveRequest authorized_request(DriveRequest requested, float yaw) const {
// +    // Recovery proved an axle pivot, not the caller's optional forward feed.
// +    // A fresh rolling turn is handled separately before this authorization.
// +    requested.linear_m_s = 0;
//      const float remaining = remaining_angle(yaw);
//      // Near +/-pi, executor heading noise can reverse the shortest-angle sign.
//      // Apply the direction of the arc actually proved by this guard. Small
// @@ -440,31 +496,128 @@
//      }
//      return true;
//    }
// -  bool ranges_clear(const loc::PoseSnapshot &p, const BootstrapFrame &f,
// -                    float cardinal) const {
// -    float required[3]{};
// +  template <typename Sample>
// +  bool measured_sweep_clear(const loc::MazeMap &map, const loc::PoseSnapshot &p,
// +                            const BootstrapFrame &f, int steps,
// +                            Sample sample) const {
// +    if (steps <= 0)
// +      return false;
// +    struct Segment {
// +      float ax, ay, bx, by;
// +    } segments[3];
// +    unsigned count = 0;
// +    for (int i = 0; i < 3; ++i) {
// +      const auto &mount = loc::cfg::lidar_mounts[i];
// +      const auto hit = loc::trace_maze_ray(map, p.x_m, p.y_m, p.yaw_rad, mount);
// +      if (hit.status != loc::RayStatus::HIT || hit.axis < 0 || hit.axis > 1 ||
// +          hit.wall_id < hit.axis * 30 || hit.wall_id >= (hit.axis + 1) * 30)
// +        return false;
// +      const float beam = p.yaw_rad + mount.yaw_body_rad;
// +      const float component = hit.axis == 0 ? std::cos(beam) : std::sin(beam);
// +      // A grazing beam cannot reliably constrain this wall normal. Its raw
// +      // validity still gates the caller; mapped polygon clearance is required.
// +      if (std::fabs(component) < .17364818f)
// +        continue;
// +      const int edge = hit.wall_id - hit.axis * 30;
// +      const int line = edge / loc::kMazeN, first = edge % loc::kMazeN;
// +      auto state_at = [&](int span) {
// +        return hit.axis == 0 ? map.edge({line == 0 ? 0 : line - 1, span},
// +                                        line == 0 ? loc::WEST : loc::EAST)
// +                             : map.edge({span, line == 0 ? 0 : line - 1},
// +                                        line == 0 ? loc::SOUTH : loc::NORTH);
// +      };
// +      int end = first;
// +      while (end < loc::kMazeN && state_at(end) == loc::EdgeState::CLOSED)
// +        ++end;
// +      if (end == first)
// +        return false;
// +      // Retain the actual wall endpoints. Extending a front wall infinitely
// +      // would reject a triangular nose that safely turns around its end.
// +      // A shorter measured return moves that wall toward the robot; a longer
// +      // return never relaxes the independent mapped sweep.
// +      const float distance = std::min(f.range_m[i], hit.range);
// +      const float normal = hit.line_m + component * (distance - hit.range);
// +      const float c = std::cos(p.yaw_rad), sn = std::sin(p.yaw_rad);
// +      const float along_origin =
// +          hit.axis == 0 ? p.y_m + sn * mount.x_body_m + c * mount.y_body_m
// +                        : p.x_m + c * mount.x_body_m - sn * mount.y_body_m;
// +      const float along_beam = hit.axis == 0 ? std::sin(beam) : std::cos(beam);
// +      const float along_hit = along_origin + along_beam * distance;
// +      const float low = std::min(first * loc::kCellM, along_hit);
// +      const float high = std::max(end * loc::kCellM, along_hit);
// +      segments[count++] = hit.axis == 0 ? Segment{normal, low, normal, high}
// +                                        : Segment{low, normal, high, normal};
// +    }
// +    if (!count)
// +      return false;
// +    for (int step = 0; step <= steps; ++step) {
// +      const auto point = sample(float(step) / steps);
// +      const float c = std::cos(point.yaw_rad), sn = std::sin(point.yaw_rad);
// +      for (unsigned i = 0; i < count; ++i) {
// +        const auto wall = segments[i];
// +        const float ax = wall.ax - point.x_m, ay = wall.ay - point.y_m;
// +        const float bx = wall.bx - point.x_m, by = wall.by - point.y_m;
// +        // Test the complete tip/shoulder/rear envelope against each observed
// +        // finite wall, including its endpoint and 2 mm clearance reserve.
// +        if (robot::geometry::footprint_intersects_segment(
// +                {c * ax + sn * ay, -sn * ax + c * ay},
// +                {c * bx + sn * by, -sn * bx + c * by}, .002f))
// +          return false;
// +      }
// +    }
// +    return true;
// +  }
// +  bool ranges_clear(const loc::MazeMap &map, const loc::PoseSnapshot &p,
// +                    const BootstrapFrame &f, float travel_per_rad = 0) const {
//      const float angle = remaining_angle(p.yaw_rad);
// -    const int steps = 1 + int(std::fabs(angle) / loc::deg2rad(1));
// -    const float axle = motor::cfg::wheel_mount_x_body_m;
// -    for (int i = 0; i <= steps; ++i) {
// -      const float a = p.yaw_rad + angle * float(i) / steps;
// -      const float dx = axle * (std::cos(p.yaw_rad) - std::cos(a));
// -      const float dy = axle * (std::sin(p.yaw_rad) - std::sin(a));
// -      const float c = std::cos(a - cardinal), s = std::sin(a - cardinal);
// -      const float forward = std::cos(cardinal) * dx + std::sin(cardinal) * dy;
// -      const float lateral = -std::sin(cardinal) * dx + std::cos(cardinal) * dy;
// -      const float forward_extent = robot::geometry::footprint_support(c, -s);
// -      const float left_extent = robot::geometry::footprint_support(s, c);
// -      const float right_extent = robot::geometry::footprint_support(-s, -c);
// -      required[0] = std::max(required[0], lateral + left_extent + .002f);
// -      required[1] = std::max(required[1], forward + forward_extent + .002f);
// -      required[2] = std::max(required[2], -lateral + right_extent + .002f);
// -    }
// -    const float error = loc::wrap_pi(p.yaw_rad - cardinal);
// -    for (int i = 0; i < 3; ++i)
// -      if (cardinal_wall_clearance(f.range_m[i], loc::cfg::lidar_mounts[i],
// -                                  error) < required[i])
// +    return measured_sweep_clear(
// +        map, p, f, turn_arc_samples(angle, travel_per_rad), [&](float t) {
// +          return turn_arc_pose(p.x_m, p.y_m, p.yaw_rad, angle * t,
// +                               travel_per_rad);
// +        });
// +  }
// +  bool recovery_ranges_clear(const loc::MazeMap &map,
// +                             const loc::PoseSnapshot &p,
// +                             const BootstrapFrame &f, float x, float y,
// +                             float yaw) const {
// +    const float angle = loc::wrap_pi(yaw - p.yaw_rad);
// +    const int steps = 1 + int(std::max(std::hypot(x - p.x_m, y - p.y_m) / .001f,
// +                                       std::fabs(angle) / loc::deg2rad(1)));
// +    return measured_sweep_clear(map, p, f, steps, [&](float t) {
// +      return TurnArcPose{p.x_m + t * (x - p.x_m), p.y_m + t * (y - p.y_m),
// +                         p.yaw_rad + t * angle};
// +    });
// +  }
// +  static bool pivot_exit_clear(const loc::MazeMap &map, float x, float y,
// +                               float yaw, float angle, float margin = .002f) {
// +    if (!mapped_observable_axle_pivot_clear(map, x, y, yaw, angle, margin))
// +      return false;
// +    const auto exit = turn_arc_pose(x, y, yaw, angle);
// +    const auto front = loc::trace_maze_ray(
// +        map, exit.x_m, exit.y_m, exit.yaw_rad, loc::cfg::lidar_mounts[1]);
// +    if (front.status != loc::RayStatus::HIT)
// +      return false;
// +    // A rear-axle turn can leave a side sensor outside the next corridor until
// +    // forward travel carries it past a finite wall end. Check up to 60 mm of
// +    // exit, shortening it to retain a 25 mm nose/front-wall stop reserve.
// +    // This selects a safe turning posture; it does not command that travel.
// +    const float travel = std::clamp(
// +        robot::geometry::front_edge_range(front.range) - .025f, 0.f, .060f);
// +    const int steps = 1 + int(travel / .001f);
// +    for (int step = 0; step <= steps; ++step) {
// +      const float distance = travel * float(step) / steps;
// +      const float px = exit.x_m + distance * std::cos(exit.yaw_rad);
// +      const float py = exit.y_m + distance * std::sin(exit.yaw_rad);
// +      if (!mapped_footprint_clear(map, px, py, exit.yaw_rad, margin))
//          return false;
// +      for (const auto &mount : loc::cfg::lidar_mounts) {
// +        const auto hit = loc::trace_maze_ray(map, px, py, exit.yaw_rad, mount);
// +        if (hit.status != loc::RayStatus::HIT ||
// +            hit.range <= loc::cfg::tof_min_m + .002f ||
// +            hit.range >= loc::cfg::tof_max_m)
// +          return false;
// +      }
// +    }
//      return true;
//    }
//    bool plan(const loc::MazeMap &map, const loc::PoseSnapshot &p,
// @@ -491,8 +644,8 @@
//          const float distance = (step + .5f * refinement) * .00025f;
//          const float x = p.x_m + distance * std::cos(p.yaw_rad);
//          const float y = p.y_m + distance * std::sin(p.yaw_rad);
// -        if (mapped_observable_axle_pivot_clear(
// -                map, x, y, p.yaw_rad, remaining_angle(p.yaw_rad), .0024f) &&
// +        if (pivot_exit_clear(map, x, y, p.yaw_rad, remaining_angle(p.yaw_rad),
// +                             .0024f) &&
//              sweep(map, p.x_m, p.y_m, p.yaw_rad, x, y, p.yaw_rad)) {
//            if (!straight_found)
//              first = distance;
// @@ -508,8 +661,8 @@
//        const float y = p.y_m + distance * std::sin(p.yaw_rad);
//        // Recheck the midpoint: disconnected feasible intervals cannot license
//        // an unproved pose between their endpoints.
// -      if (mapped_observable_axle_pivot_clear(
// -              map, x, y, p.yaw_rad, remaining_angle(p.yaw_rad), .0024f) &&
// +      if (pivot_exit_clear(map, x, y, p.yaw_rad, remaining_angle(p.yaw_rad),
// +                           .0024f) &&
//            sweep(map, p.x_m, p.y_m, p.yaw_rad, x, y, p.yaw_rad)) {
//          straight_ = true;
//          base_ = p.yaw_rad;
// @@ -532,8 +685,8 @@
//            continue;
//          const float x = p.x_m + lx * offset + std::cos(base_) * longitudinal;
//          const float y = p.y_m + ly * offset + std::sin(base_) * longitudinal;
// -        if (mapped_observable_axle_pivot_clear(
// -                map, x, y, base_, remaining_angle(base_), .0024f)) {
// +        if (pivot_exit_clear(map, x, y, base_, remaining_angle(base_),
// +                             .0024f)) {
//            if (!found)
//              first_offset = offset;
//            last_offset = offset;
// @@ -550,9 +703,8 @@
//            continue;
//          finish_x_ = p.x_m + lx * offset + std::cos(base_) * longitudinal;
//          finish_y_ = p.y_m + ly * offset + std::sin(base_) * longitudinal;
// -        if (!mapped_observable_axle_pivot_clear(map, finish_x_, finish_y_,
// -                                                base_, remaining_angle(base_),
// -                                                .0024f))
// +        if (!pivot_exit_clear(map, finish_x_, finish_y_, base_,
// +                              remaining_angle(base_), .0024f))
//            continue;
//          for (float degrees : {16.f, 14.f, 12.f})
//            for (float direction : {1.f, -1.f}) {
// @@ -593,4 +745,5 @@
//    }
//  #endif
//  };
// +static_assert(sizeof(TurnGuard) <= 128, "Bounded task-owned guard state");
//  } // namespace nav
//
// --- a/fw/robot_sf/include/navigation/turn_clearance.hpp
// +++ b/fw/robot_sf/include/navigation/turn_clearance.hpp
// @@ -54,6 +54,60 @@
//    return true;
//  }
//
// +// Wheel twist is specified at the rear axle. Include its forward travel as
// +// well as the chassis-origin displacement around it during a rolling turn.
// +struct TurnArcPose {
// +  float x_m, y_m, yaw_rad;
// +};
// +inline TurnArcPose turn_arc_pose(float x, float y, float yaw, float angle,
// +                                 float travel_per_rad = 0) {
// +  const float a = yaw + angle;
// +  const float axle = motor::cfg::wheel_mount_x_body_m;
// +  return {x + axle * (std::cos(yaw) - std::cos(a)) +
// +              travel_per_rad * (std::sin(a) - std::sin(yaw)),
// +          y + axle * (std::sin(yaw) - std::sin(a)) -
// +              travel_per_rad * (std::cos(a) - std::cos(yaw)),
// +          a};
// +}
// +inline int turn_arc_samples(float angle, float travel_per_rad = 0) {
// +  const float travel = std::fabs(angle * travel_per_rad);
// +  // Bound work and avoid converting non-finite/oversized counts to int. An
// +  // arc longer than two metres cannot be relied on inside this metre arena.
// +  if (!std::isfinite(angle) || !std::isfinite(travel_per_rad) ||
// +      std::fabs(angle) > 2 * loc::kPi || !std::isfinite(travel) || travel > 2)
// +    return 0;
// +  return 1 + int(std::max(std::fabs(angle) / loc::deg2rad(1), travel / .001f));
// +}
// +inline bool mapped_rolling_turn_clear(const loc::MazeMap &map, float x, float y,
// +                                      float yaw, float angle,
// +                                      float travel_per_rad,
// +                                      float margin = .002f) {
// +  const int steps = turn_arc_samples(angle, travel_per_rad);
// +  if (!steps)
// +    return false;
// +  for (int i = 0; i <= steps; ++i) {
// +    const auto p =
// +        turn_arc_pose(x, y, yaw, angle * float(i) / steps, travel_per_rad);
// +    if (!mapped_footprint_clear(map, p.x_m, p.y_m, p.yaw_rad, margin))
// +      return false;
// +    unsigned axes = 0;
// +    for (const auto &mount : loc::cfg::lidar_mounts) {
// +      const auto hit = loc::trace_maze_ray(map, p.x_m, p.y_m, p.yaw_rad, mount);
// +      if (hit.status != loc::RayStatus::HIT || hit.ambiguous || hit.axis < 0 ||
// +          hit.axis > 1 || hit.range <= loc::cfg::tof_min_m + .002f ||
// +          hit.range >= loc::cfg::tof_max_m)
// +        return false;
// +      const float beam = p.yaw_rad + mount.yaw_body_rad;
// +      const float component = hit.axis == 0 ? std::cos(beam) : std::sin(beam);
// +      if (std::fabs(component) >= .17364818f) // sin(10 deg), as in FEATURE.
// +        axes |= 1u << hit.axis;
// +    }
// +    if (axes != 3)
// +      return false;
// +  }
// +  return true;
// +}
// +
//  inline bool mapped_axle_pivot_clear(const loc::MazeMap &map, float x, float y,
//                                      float yaw, float angle,
//                                      float margin = .002f) {
//
// --- a/fw/autonomous/STM32_TINYRCP/src/tasks/navigation_tasks.cpp
// +++ b/fw/autonomous/STM32_TINYRCP/src/tasks/navigation_tasks.cpp
// @@ -698,6 +698,9 @@
//              turn_guard.cancel();
//              turn_relocation.cancel();
//            } else if (rhr_startup_turn_authorized) {
// +            // The all-hypothesis startup proof covers a stationary axle turn.
// +            // Never apply rolling feed while translation is unlocalized.
// +            drive.linear_m_s = 0;
//              turn_guard.cancel();
//              turn_relocation.cancel();
//            } else if (using_fallback && !state.feature.pose.tracking &&
// @@ -724,10 +727,11 @@
//                          turn_guard.phase() == TurnGuardPhase::BLOCKED
//                      ? robot_log::Level::WARN
//                      : robot_log::Level::INFO,
// -                TAG, "turn guard %s -> %s side center L/R=%.3f/%.3f",
// +                TAG, "turn guard %s -> %s center L/F/R=%.3f/%.3f/%.3f",
//                  turn_guard_phase_name(reported_guard),
//                  turn_guard_phase_name(turn_guard.phase()),
//                  double(turn_guard.left_clearance_m()),
// +                double(turn_guard.front_clearance_m()),
//                  double(turn_guard.right_clearance_m()));
//              reported_guard = turn_guard.phase();
//            }
