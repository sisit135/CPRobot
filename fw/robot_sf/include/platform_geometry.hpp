// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "motor_control/tt90_reference.hpp"
#include <array>
#include <cstddef>

// Mounting drawing: +X right, +Y forward; retain the original platform origin.
// Navigation body frame: +x forward, +y left. Convert only at its boundary.
#if AXI_PLATFORM_PROTOTYPE || AXI_PLATFORM_V1
#error "V1/prototype platform is deprecated; use AXI_PLATFORM_V2=1"
#endif
#ifndef AXI_PLATFORM_V2
#define AXI_PLATFORM_V2 1
#endif
#if !AXI_PLATFORM_V2
#error "V1 platform is deprecated; AXI_PLATFORM_V2 must be enabled"
#endif
namespace robot::geometry {
struct MountPoint {
  float x_m, y_m;
};
struct BodyPoint {
  float x_body_m, y_body_m;
};
constexpr BodyPoint to_body(MountPoint p) { return {p.y_m, -p.x_m}; }
constexpr float root(float v) {
  float r = 1;
  for (int i = 0; i < 20; ++i)
    r = (r + v / r) / 2;
  return r;
}
constexpr float prototype_half_side_m = .190f / 2.82842712474619f;
// Legacy alias for original sensor offsets; footprint dimensions are separate.
constexpr float platform_half_side_m = prototype_half_side_m;
constexpr float wheel_mount_y_m = -.024f;
// Provisional 65 mm diameter, 26 mm width; direct TT output shaft attachment.
constexpr float wheel_width_m = .026f;
constexpr float track_width_m = 2 * prototype_half_side_m - wheel_width_m;
constexpr float side_tof_y_m = .0225f;

constexpr bool platform_v2 = true;
constexpr float platform_width_m = .135f, platform_length_m = .110f;
constexpr MountPoint front_tof_mount = {.0022f, .0316f};
constexpr float platform_half_width_m = platform_width_m / 2;
constexpr float platform_half_length_m = platform_length_m / 2;
constexpr float platform_diagonal_m =
    root(platform_width_m * platform_width_m +
         platform_length_m * platform_length_m);

// Left/right wheels; left/front/right ToFs. Side mounts remain original.
constexpr MountPoint wheel_mounts[2] = {{-track_width_m / 2, wheel_mount_y_m},
                                        {+track_width_m / 2, wheel_mount_y_m}};
constexpr MountPoint tof_mounts[3] = {{-prototype_half_side_m, side_tof_y_m},
                                      front_tof_mount,
                                      {+prototype_half_side_m, side_tof_y_m}};
// Head coordinates are absolute mounting coordinates. The shoulders retain
// the side-ToF X positions, at Y=+23 mm from the original origin.
constexpr float head_base_y_m = .023f;
constexpr std::array<MountPoint, 3> head_points = {{
    {tof_mounts[0].x_m, head_base_y_m},
    {0, platform_half_length_m},
    {tof_mounts[2].x_m, head_base_y_m},
}};
// Retain the full-width rectangular lower body. The 0.325 mm shoulders join
// its sides to the triangle without moving the sensors or narrowing the base.
constexpr std::array<MountPoint, 7> platform_outline = {{
    {-platform_half_width_m, -platform_half_length_m},
    {platform_half_width_m, -platform_half_length_m},
    {platform_half_width_m, head_base_y_m},
    head_points[2],
    head_points[1],
    head_points[0],
    {-platform_half_width_m, head_base_y_m},
}};
static_assert(
    head_base_y_m > side_tof_y_m && head_base_y_m < platform_half_length_m,
    "Head shoulders must be ahead of the side ToFs and behind the tip");

// Front-wall clearance measured from the leading tip plane (Y=+55 mm).
// Keep the nose's full margin despite the front sensor's lateral offset;
// localization still consumes the raw range at the original sensor mount.
constexpr float front_edge_range(float sensor_range_m) {
  return sensor_range_m + front_tof_mount.y_m - platform_half_length_m;
}
constexpr float wheel_mount_x_body_m = to_body(wheel_mounts[0]).x_body_m;
constexpr float side_tof_x_body_m = to_body(tof_mounts[0]).x_body_m;
// Keep the conservative full-width rear tire envelope. Its rear extent is
// unchanged; the forward envelope follows the triangular platform head.
constexpr float rear_tire_m = motor::tt90::wheel_radius_m - wheel_mount_y_m;
constexpr float footprint_rear_m =
    rear_tire_m > platform_half_length_m ? rear_tire_m : platform_half_length_m;
constexpr float footprint_front_m = platform_half_length_m;
constexpr float footprint_half_width_m = platform_half_width_m;
constexpr float footprint_radius_m =
    root(footprint_rear_m * footprint_rear_m +
         footprint_half_width_m * footprint_half_width_m);
// Boundary planes use the navigation body frame: n dot p <= limit.
struct BodyPlane {
  float nx, ny, limit;
};
constexpr float head_half_width_m = head_points[2].x_m;
constexpr float head_height_m = platform_half_length_m - head_base_y_m;
constexpr float head_slope_length_m =
    root(head_half_width_m * head_half_width_m + head_height_m * head_height_m);
constexpr float head_nx = head_half_width_m / head_slope_length_m;
constexpr float head_ny = head_height_m / head_slope_length_m;
constexpr std::array<BodyPlane, 4> footprint_base_planes = {{
    {1, 0, head_base_y_m},
    {-1, 0, footprint_rear_m},
    {0, 1, footprint_half_width_m},
    {0, -1, footprint_half_width_m},
}};
constexpr std::array<BodyPlane, 3> head_planes = {{
    {-1, 0, -head_base_y_m},
    {head_nx, head_ny, head_nx * footprint_front_m},
    {head_nx, -head_ny, head_nx * footprint_front_m},
}};
constexpr std::array<BodyPoint, 7> footprint_vertices = {{
    {-footprint_rear_m, footprint_half_width_m},
    {-footprint_rear_m, -footprint_half_width_m},
    to_body(platform_outline[2]),
    to_body(platform_outline[3]),
    to_body(platform_outline[4]),
    to_body(platform_outline[5]),
    to_body(platform_outline[6]),
}};
// Maximum projection of the actual head and conservative rear envelope.
constexpr float footprint_support(float nx, float ny) {
  float extent = -1;
  for (auto p : footprint_vertices) {
    const float d = nx * p.x_body_m + ny * p.y_body_m;
    if (d > extent)
      extent = d;
  }
  return extent;
}
template <std::size_t N>
constexpr bool segment_intersects_planes(BodyPoint a, BodyPoint b,
                                         const std::array<BodyPlane, N> &planes,
                                         float margin) {
  float first = 0, last = 1;
  for (std::size_t i = 0; i < N; ++i) {
    const auto plane = planes[i];
    // Plane 0 is the shared base/head seam. Do not open a false gap when
    // applying the simulator's optional inward scrape tolerance. Retaining
    // this seam is conservative near the small exposed shoulder ledges.
    const float padding = margin < 0 && i == 0 ? 0 : margin;
    const float u =
        plane.limit + padding - (plane.nx * a.x_body_m + plane.ny * a.y_body_m);
    const float v =
        plane.limit + padding - (plane.nx * b.x_body_m + plane.ny * b.y_body_m);
    if (u < 0 && v < 0)
      return false;
    if (u < 0) {
      const float crossing = u / (u - v);
      if (crossing > first)
        first = crossing;
    } else if (v < 0) {
      const float crossing = u / (u - v);
      if (crossing < last)
        last = crossing;
    }
    if (first > last)
      return false;
  }
  return true;
}
constexpr bool footprint_intersects_segment(BodyPoint a, BodyPoint b,
                                            float margin = 0) {
  return segment_intersects_planes(a, b, footprint_base_planes, margin) ||
         segment_intersects_planes(a, b, head_planes, margin);
}
} // namespace robot::geometry
