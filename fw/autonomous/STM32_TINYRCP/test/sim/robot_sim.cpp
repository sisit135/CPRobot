// SPDX-License-Identifier: GPL-3.0-only

#include "robot_sim.hpp"
#include "localization/config.hpp"
#include "motor_control/config.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace sim {
Robot::Robot(loc::MazeMap m, loc::Cell c, float a, uint32_t s, Noise n, Plant p,
             Surface surface)
    : x((c.x + .5f) * .2f), y((c.y + .5f) * .2f), yaw(a), map_(m), noise_(n),
      plant_(p), surface_(surface), rng_(s) {
  if (!surface_.enabled)
    return;
  const float values[] = {surface_.mass_kg,
                          surface_.chassis_diagonal_m,
                          surface_.mu_static,
                          surface_.mu_kinetic,
                          surface_.rolling_resistance_coefficient,
                          surface_.contact_left_fraction,
                          surface_.contact_right_fraction,
                          surface_.wheel_rim_mass_kg,
                          surface_.motor_rotor_inertia_kg_m2};
  for (float v : values)
    if (!std::isfinite(v))
      throw std::invalid_argument("surface parameters must be finite");
  if (surface_.mass_kg <= 0 || surface_.chassis_diagonal_m <= 0 ||
      surface_.mu_kinetic < 0 || surface_.mu_static < surface_.mu_kinetic ||
      surface_.mu_static > 5.f || surface_.rolling_resistance_coefficient < 0 ||
      surface_.rolling_resistance_coefficient > 1.f ||
      surface_.contact_left_fraction < 0 ||
      surface_.contact_left_fraction > 1 ||
      surface_.contact_right_fraction < 0 ||
      surface_.contact_right_fraction > 1 || surface_.wheel_rim_mass_kg < 0 ||
      surface_.motor_rotor_inertia_kg_m2 < 0 || surface_.mass_kg > 1000.f ||
      surface_.chassis_diagonal_m > 10.f || surface_.wheel_rim_mass_kg > 10.f ||
      surface_.motor_rotor_inertia_kg_m2 > 1.f)
    throw std::invalid_argument("invalid surface parameter range");
  body_inertia_kg_m2_ = surface_.mass_kg * surface_.chassis_diagonal_m *
                        surface_.chassis_diagonal_m / 12.f;
  wheel_inertia_kg_m2_ = surface_.wheel_rim_mass_kg *
                             motor::tt90::wheel_radius_m *
                             motor::tt90::wheel_radius_m +
                         surface_.motor_rotor_inertia_kg_m2 *
                             motor::tt90::gear_ratio * motor::tt90::gear_ratio;
  if (!std::isfinite(body_inertia_kg_m2_) ||
      !std::isfinite(wheel_inertia_kg_m2_) || !(body_inertia_kg_m2_ > 0) ||
      !(wheel_inertia_kg_m2_ > 0))
    throw std::invalid_argument("surface inertia must be positive");
}
float Robot::gaussian(float sigma) { return sigma * normal_(rng_); }
float Robot::range(const loc::LidarMount &m) const {
  const float c = std::cos(yaw), s = std::sin(yaw),
              ox = x + c * m.x_body_m - s * m.y_body_m,
              oy = y + s * m.x_body_m + c * m.y_body_m;
  const float dx = std::cos(yaw + m.yaw_body_rad),
              dy = std::sin(yaw + m.yaw_body_rad);
  int cx = int(std::floor(ox / .2f)), cy = int(std::floor(oy / .2f));
  if (cx < 0 || cx >= 5 || cy < 0 || cy >= 5)
    return NAN;
  // Direct access to raw masks, independent edge implementation.
  auto closed = [&](int a, int b, uint8_t bit, int na, int nb,
                    uint8_t opposite) {
    bool wall = map_.walls[4 - b][a] & bit;
    if (na >= 0 && na < 5 && nb >= 0 && nb < 5)
      wall = wall || (map_.walls[4 - nb][na] & opposite);
    return wall;
  };
  for (int step = 0; step < 12; ++step) {
    const float tx = std::fabs(dx) < 1e-6f
                         ? INFINITY
                         : ((dx > 0 ? (cx + 1) * .2f : cx * .2f) - ox) / dx;
    const float ty = std::fabs(dy) < 1e-6f
                         ? INFINITY
                         : ((dy > 0 ? (cy + 1) * .2f : cy * .2f) - oy) / dy;
    if (tx < ty) {
      int nx = cx + (dx > 0 ? 1 : -1);
      if (closed(cx, cy, dx > 0 ? loc::EAST : loc::WEST, nx, cy,
                 dx > 0 ? loc::WEST : loc::EAST))
        return tx;
      cx = nx;
    } else {
      int ny = cy + (dy > 0 ? 1 : -1);
      if (closed(cx, cy, dy > 0 ? loc::NORTH : loc::SOUTH, cx, ny,
                 dy > 0 ? loc::SOUTH : loc::NORTH))
        return ty;
      cy = ny;
    }
    if (cx < 0 || cx >= 5 || cy < 0 || cy >= 5)
      return NAN;
  }
  return NAN;
}
void Robot::step(float l, float r, float dt) {
  if (surface_.enabled) {
    step_surface(l, r, dt);
    return;
  }
  auto speed = [](float duty, float gain, float offset) {
    return std::copysign(
        gain * std::max(0.f, std::fabs(std::clamp(duty, -1.f, 1.f)) - offset),
        duty);
  };
  wl += (speed(l, plant_.left_gain, plant_.left_duty_offset) - wl) * dt /
        plant_.left_tau_s;
  wr += (speed(r, plant_.right_gain, plant_.right_duty_offset) - wr) * dt /
        plant_.right_tau_s;
  const float v = (wl + wr) * motor::cfg::wheel_radius_m / 2;
  omega_ = (wr - wl) * motor::cfg::wheel_radius_m / motor::cfg::track_width_m;
  // The wheel axle is behind the reported platform center. No lateral slip
  // at that axle gives center lateral velocity -axle_x * yaw_rate.
  const float lateral = -motor::cfg::wheel_mount_x_body_m * omega_;
  const float mid_yaw = yaw + omega_ * dt / 2;
  const float nx = x +
                   (v * std::cos(mid_yaw) - lateral * std::sin(mid_yaw)) * dt,
              ny = y +
                   (v * std::sin(mid_yaw) + lateral * std::cos(mid_yaw)) * dt;
  const loc::Cell before{int(std::floor(x / .2f)), int(std::floor(y / .2f))},
      after{int(std::floor(nx / .2f)), int(std::floor(ny / .2f))};
  if (!loc::valid_cell(after) ||
      (before != after && !map_.can_step(before, after)))
    collided = true;
  x = nx;
  y = ny;
  yaw = loc::wrap_pi(yaw + omega_ * dt);
  vx_m_s = v * std::cos(yaw) - lateral * std::sin(yaw);
  vy_m_s = v * std::sin(yaw) + lateral * std::cos(yaw);
  body_omega_rps = omega_;
}
void Robot::step_surface(float left, float right, float dt) {
  if (!std::isfinite(dt) || dt <= 0 || !std::isfinite(left) ||
      !std::isfinite(right))
    throw std::invalid_argument("surface step requires finite duty and dt>0");
  constexpr float max_step_s = .001f;
  constexpr float half_track_m = motor::cfg::track_width_m / 2.f;
  constexpr float axle_x_m = motor::cfg::wheel_mount_x_body_m;
  constexpr float gravity_mps2 = 9.80665f;
  constexpr float wheel_radius_m = motor::tt90::wheel_radius_m;
  constexpr float gear_coulomb_nm =
      motor::tt90::torque_nm_per_a * motor::tt90::no_load_current_a;
  const float substeps_f = std::ceil(dt / max_step_s);
  if (!std::isfinite(substeps_f) || substeps_f > 1.e7f)
    throw std::invalid_argument("surface timestep is too large");
  const int substeps = std::max(1, static_cast<int>(substeps_f));
  const float h = dt / substeps;
  const float left_normal =
      surface_.mass_kg * gravity_mps2 * .5f * surface_.contact_left_fraction;
  const float right_normal =
      surface_.mass_kg * gravity_mps2 * .5f * surface_.contact_right_fraction;
  const loc::Cell before{int(std::floor(x / .2f)), int(std::floor(y / .2f))};
  float left_impulse_sum = 0, right_impulse_sum = 0;
  float left_lateral_impulse_sum = 0, right_lateral_impulse_sum = 0;

  auto update_wheel = [&](float duty, float &wheel_speed) {
    duty = std::clamp(duty, -1.f, 1.f);
    // DRV8833 coast at zero PWM has no electrical braking current. A driven
    // wheel uses the reference DC motor equation at the geared output shaft.
    float motor_torque = 0;
    if (std::fabs(duty) > 1.e-6f) {
      const float voltage = duty * motor::tt90::supply_v;
      const float current = std::clamp(
          (voltage - motor::tt90::back_emf_v_per_rad_s * wheel_speed) /
              motor::tt90::total_resistance_ohm,
          -motor::tt90::bridge_peak_current_a,
          motor::tt90::bridge_peak_current_a);
      motor_torque = motor::tt90::torque_nm_per_a * current;
    }
    // No-load current is modeled as passive gear/motor loss. Bound the update
    // at zero speed so a finite coast step cannot reverse a stopped wheel.
    if (wheel_speed == 0 && std::fabs(motor_torque) <= gear_coulomb_nm)
      return;
    const float friction_sign = wheel_speed == 0
                                    ? std::copysign(1.f, motor_torque)
                                    : std::copysign(1.f, wheel_speed);
    const float delta = (motor_torque - friction_sign * gear_coulomb_nm) * h /
                        wheel_inertia_kg_m2_;
    if (wheel_speed != 0 && wheel_speed * (wheel_speed + delta) < 0 &&
        std::fabs(motor_torque) <= gear_coulomb_nm)
      wheel_speed = 0;
    else
      wheel_speed += delta;
  };

  for (int i = 0; i < substeps; ++i) {
    update_wheel(left, wl);
    update_wheel(right, wr);

    // Each wheel resolves a 2D Coulomb friction circle. Longitudinal contact
    // couples wheel spin, body translation, and yaw; lateral contact damps
    // sideways motion. Neither effect is injected as an artificial pose.
    const float inv_body_long =
        1.f / surface_.mass_kg +
        half_track_m * half_track_m / body_inertia_kg_m2_;
    const float inv_effective_long =
        inv_body_long + wheel_radius_m * wheel_radius_m / wheel_inertia_kg_m2_;
    const float inv_effective_lateral =
        1.f / surface_.mass_kg + axle_x_m * axle_x_m / body_inertia_kg_m2_;
    float left_long_j = 0, right_long_j = 0;
    float left_lat_j = 0, right_lat_j = 0;
    for (int iteration = 0; iteration < 6; ++iteration) {
      auto solve_tire = [&](bool is_left, float normal, float &total_long_j,
                            float &total_lat_j) {
        if (normal <= 0)
          return;
        const float side = is_left ? 1.f : -1.f;
        const float c = std::cos(yaw), s = std::sin(yaw);
        const float forward_speed = vx_m_s * c + vy_m_s * s;
        const float lateral_speed = -vx_m_s * s + vy_m_s * c;
        const float body_contact_speed =
            forward_speed - side * half_track_m * body_omega_rps;
        const float body_contact_lateral =
            lateral_speed + axle_x_m * body_omega_rps;
        const float wheel_speed = is_left ? wl : wr;
        const float slip_long =
            wheel_radius_m * wheel_speed - body_contact_speed;
        const float slip_lat = -body_contact_lateral;
        if (std::fabs(slip_long) < 1.e-7f && std::fabs(slip_lat) < 1.e-7f)
          return;
        // A rearward contact couples lateral and longitudinal impulses
        // through yaw. Solve the full 2x2 contact effective-mass matrix.
        const float cross =
            -side * half_track_m * axle_x_m / body_inertia_kg_m2_;
        const float determinant =
            inv_effective_long * inv_effective_lateral - cross * cross;
        const float requested_long =
            total_long_j +
            (inv_effective_lateral * slip_long - cross * slip_lat) /
                determinant;
        const float requested_lat =
            total_lat_j +
            (inv_effective_long * slip_lat - cross * slip_long) / determinant;
        const float static_limit = surface_.mu_static * normal * h;
        const float kinetic_limit = surface_.mu_kinetic * normal * h;
        // Stick if the combined impulse fits inside the static circle. During
        // slip, cap the vector at mu_k*N*dt, never either axis independently.
        const float requested_norm = std::hypot(requested_long, requested_lat);
        float next_long = requested_long, next_lat = requested_lat;
        if (requested_norm > static_limit && requested_norm > 0) {
          const float scale = kinetic_limit / requested_norm;
          next_long *= scale;
          next_lat *= scale;
        }
        const float impulse_long = next_long - total_long_j;
        const float impulse_lat = next_lat - total_lat_j;
        total_long_j = next_long;
        total_lat_j = next_lat;
        vx_m_s += (impulse_long * c - impulse_lat * s) / surface_.mass_kg;
        vy_m_s += (impulse_long * s + impulse_lat * c) / surface_.mass_kg;
        body_omega_rps +=
            (axle_x_m * impulse_lat - side * half_track_m * impulse_long) /
            body_inertia_kg_m2_;
        if (is_left)
          wl -= wheel_radius_m * impulse_long / wheel_inertia_kg_m2_;
        else
          wr -= wheel_radius_m * impulse_long / wheel_inertia_kg_m2_;
      };
      solve_tire(true, left_normal, left_long_j, left_lat_j);
      solve_tire(false, right_normal, right_long_j, right_lat_j);
    }
    left_impulse_sum += left_long_j;
    right_impulse_sum += right_long_j;
    left_lateral_impulse_sum += left_lat_j;
    right_lateral_impulse_sum += right_lat_j;

    // Rolling resistance opposes the full contact velocity, with the same
    // Coulomb-circle cap and a proportional stop impulse to prevent reversal.
    auto apply_rolling = [&](bool is_left, float normal) {
      if (normal <= 0 || surface_.rolling_resistance_coefficient <= 0)
        return;
      const float side = is_left ? 1.f : -1.f;
      const float c = std::cos(yaw), s = std::sin(yaw);
      const float forward_speed = vx_m_s * c + vy_m_s * s;
      const float lateral_speed = -vx_m_s * s + vy_m_s * c;
      const float contact_long =
          forward_speed - side * half_track_m * body_omega_rps;
      const float contact_lat = lateral_speed + axle_x_m * body_omega_rps;
      if (std::hypot(contact_long, contact_lat) < 1.e-8f)
        return;
      const float cross = -side * half_track_m * axle_x_m / body_inertia_kg_m2_;
      const float determinant =
          inv_body_long * inv_effective_lateral - cross * cross;
      const float stop_long =
          (-inv_effective_lateral * contact_long + cross * contact_lat) /
          determinant;
      const float stop_lat =
          (-inv_body_long * contact_lat + cross * contact_long) / determinant;
      const float stop_norm = std::hypot(stop_long, stop_lat);
      const float max_j = surface_.rolling_resistance_coefficient * normal * h;
      const float scale = stop_norm > max_j ? max_j / stop_norm : 1.f;
      const float impulse_long = stop_long * scale;
      const float impulse_lat = stop_lat * scale;
      vx_m_s += (impulse_long * c - impulse_lat * s) / surface_.mass_kg;
      vy_m_s += (impulse_long * s + impulse_lat * c) / surface_.mass_kg;
      body_omega_rps +=
          (axle_x_m * impulse_lat - side * half_track_m * impulse_long) /
          body_inertia_kg_m2_;
    };
    apply_rolling(true, left_normal);
    apply_rolling(false, right_normal);

    x += vx_m_s * h;
    y += vy_m_s * h;
    yaw = loc::wrap_pi(yaw + body_omega_rps * h);
  }

  force_left_n = left_impulse_sum / dt;
  force_right_n = right_impulse_sum / dt;
  force_left_lat_n = left_lateral_impulse_sum / dt;
  force_right_lat_n = right_lateral_impulse_sum / dt;
  omega_ = body_omega_rps;
  const float c = std::cos(yaw), s = std::sin(yaw);
  const float forward_speed = vx_m_s * c + vy_m_s * s;
  const float lateral_speed = -vx_m_s * s + vy_m_s * c;
  slip_left_m_s =
      wheel_radius_m * wl - (forward_speed - half_track_m * body_omega_rps);
  slip_right_m_s =
      wheel_radius_m * wr - (forward_speed + half_track_m * body_omega_rps);
  const loc::Cell after{int(std::floor(x / .2f)), int(std::floor(y / .2f))};
  if (!loc::valid_cell(after) ||
      (before != after && !map_.can_step(before, after)))
    collided = true;
}
nav::BootstrapFrame Robot::sensors(int64_t now, uint32_t seq) {
  nav::BootstrapFrame f{};
  f.color = {800, 100, 100, 1200, seq, false};
  f.color_us = f.imu_us = f.mag_us = now;
  f.imu.gz_rps = omega_ + noise_.gyro_bias + gaussian(noise_.gyro_sigma);
  f.imu.ax_mps2 = gaussian(.01f);
  f.imu.ay_mps2 = gaussian(.01f);
  f.imu.az_mps2 = 9.80665f + gaussian(.01f);
  const float a = yaw + gaussian(noise_.mag_sigma);
  f.mag = {45 * std::cos(a), -45 * std::sin(a), 0};
  f.magnetic_clean = std::fabs(wl) < .05f && std::fabs(wr) < .05f;
  for (int i = 0; i < 3; ++i) {
    f.tof_us[i] = now;
    float distance = range(loc::cfg::lidar_mounts[i]);
    if (noise_.tof_fov_deg > 0) {
      // Nine rays span the full FOV. Model the nearest finite optical return.
      distance = INFINITY;
      for (int beam = 0; beam < 9; ++beam) {
        auto mount = loc::cfg::lidar_mounts[i];
        mount.yaw_body_rad +=
            loc::deg2rad(noise_.tof_fov_deg) * (float(beam) / 8 - .5f);
        const float hit = range(mount);
        if (std::isfinite(hit))
          distance = std::min(distance, hit);
      }
    }
    f.range_m[i] = distance + gaussian(noise_.range_sigma);
  }
  return f;
}
} // namespace sim
