// SPDX-License-Identifier: GPL-3.0-only

#include "motor_control/config.hpp"
#include "robot_sim.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
constexpr float kRadius = motor::tt90::wheel_radius_m;

sim::Robot make_robot(sim::Surface surface = {}, loc::Cell cell = {2, 2}) {
  surface.enabled = true;
  return {loc::MazeMap::q1(), cell, 0, 481, {}, {}, surface};
}
float kinetic_energy(const sim::Robot &r, const sim::Surface &s) {
  const float body_i =
      s.mass_kg * s.chassis_diagonal_m * s.chassis_diagonal_m / 12.f;
  const float wheel_i = s.wheel_rim_mass_kg * kRadius * kRadius +
                        s.motor_rotor_inertia_kg_m2 * motor::tt90::gear_ratio *
                            motor::tt90::gear_ratio;
  return .5f * s.mass_kg * (r.vx_m_s * r.vx_m_s + r.vy_m_s * r.vy_m_s) +
         .5f * body_i * r.body_omega_rps * r.body_omega_rps +
         .5f * wheel_i * (r.wl * r.wl + r.wr * r.wr);
}
void no_motion_at_rest() {
  sim::Surface surface;
  auto r = make_robot(surface);
  const float x = r.x, y = r.y, yaw = r.yaw;
  for (int i = 0; i < 100; ++i)
    r.step(0, 0, .01f);
  assert(r.x == x && r.y == y && r.yaw == yaw);
  assert(r.vx_m_s == 0 && r.vy_m_s == 0 && r.body_omega_rps == 0);
  assert(r.force_left_n == 0 && r.force_right_n == 0);
}
void rear_axle_contacts_couple_lateral_motion_and_yaw() {
  sim::Surface surface;
  surface.rolling_resistance_coefficient = 0;
  auto r = make_robot(surface);
  r.vy_m_s = .2f;
  r.step(0, 0, .001f);
  // Both tire contacts are 24 mm behind the center: negative lateral force
  // must generate positive yaw torque, rather than damping translation only.
  assert(std::fabs(motor::cfg::wheel_mount_x_body_m + .024f) < 1.e-7f);
  assert(r.force_left_lat_n < 0 && r.force_right_lat_n < 0);
  assert(r.body_omega_rps > 1.e-4f);
  assert(kinetic_energy(r, surface) < .5f * surface.mass_kg * .2f * .2f);
}
void legacy_pivot_preserves_rear_axle_center() {
  sim::Robot r(loc::MazeMap::q1(), {2, 2}, 0, 481);
  const float axle_x = r.x + motor::cfg::wheel_mount_x_body_m;
  const float axle_y = r.y;
  // Symmetric wheels remove the plant's deliberately unequal right gain/lag.
  sim::Plant plant;
  plant.right_gain = plant.left_gain;
  plant.right_tau_s = plant.left_tau_s;
  r = sim::Robot(loc::MazeMap::q1(), {2, 2}, 0, 481, {}, plant);
  for (int i = 0; i < 500; ++i)
    r.step(-.3f, .3f, .001f);
  assert(std::fabs(r.yaw) > .1f);
  assert(std::fabs(r.x + motor::cfg::wheel_mount_x_body_m * std::cos(r.yaw) -
                   axle_x) < 1.e-5f);
  assert(std::fabs(r.y + motor::cfg::wheel_mount_x_body_m * std::sin(r.yaw) -
                   axle_y) < 1.e-5f);
  assert(std::hypot(r.x - .5f, r.y - .5f) > .001f);
}
void absent_contact_only_spins_wheels() {
  sim::Surface surface;
  surface.contact_left_fraction = surface.contact_right_fraction = 0;
  auto r = make_robot(surface);
  const float x = r.x, y = r.y, yaw = r.yaw;
  r.step(.5f, .5f, .1f);
  assert(std::fabs(r.wl) > .1f && std::fabs(r.wr) > .1f);
  assert(r.x == x && r.y == y && r.yaw == yaw);
  assert(r.vx_m_s == 0 && r.vy_m_s == 0 && r.body_omega_rps == 0);
  assert(r.force_left_n == 0 && r.force_right_n == 0);
}
void traction_is_friction_limited_and_slips() {
  sim::Surface surface;
  auto r = make_robot(surface);
  r.step(1, 1, .01f);
  const float normal = surface.mass_kg * 9.80665f * .5f;
  assert(std::hypot(r.force_left_n, r.force_left_lat_n) <=
         surface.mu_static * normal + .01f);
  assert(std::hypot(r.force_right_n, r.force_right_lat_n) <=
         surface.mu_static * normal + .01f);
  assert(std::fabs(r.slip_left_m_s) > 1.e-4f);
  assert(std::fabs(r.slip_right_m_s) > 1.e-4f);
}
void moderate_drive_sticks_without_extra_contact_force() {
  sim::Surface surface;
  auto r = make_robot(surface);
  r.step(.25f, .25f, .1f);
  assert(std::fabs(r.slip_left_m_s) < 1.e-3f);
  assert(std::fabs(r.slip_right_m_s) < 1.e-3f);
  const float normal = surface.mass_kg * 9.80665f * .5f;
  assert(std::fabs(r.force_left_n) <= surface.mu_static * normal + .01f);
  assert(std::fabs(r.force_right_n) <= surface.mu_static * normal + .01f);
}
void coast_dissipates_without_reversal() {
  sim::Surface surface;
  auto r = make_robot(surface);
  r.step(.35f, .35f, .30f);
  const float moving_energy = kinetic_energy(r, surface);
  assert(moving_energy > 1.e-5f);
  r.step(0, 0, 2.f);
  const float coast_energy = kinetic_energy(r, surface);
  assert(coast_energy < moving_energy);
  assert(std::fabs(r.wl) < 1.e-3f && std::fabs(r.wr) < 1.e-3f);
  assert(std::fabs(r.vx_m_s) < 1.e-3f && std::fabs(r.vy_m_s) < 1.e-3f);
  // Contact iteration leaves nanometer/second roundoff at rest; reject a
  // physical reversal while allowing error below the solver's 1e-7 m/s gate.
  assert(r.wl >= -1e-8f && r.wr >= -1e-8f && r.vx_m_s >= -1e-8f);
}
void unequal_contact_and_repeatability() {
  sim::Surface surface;
  surface.contact_left_fraction = 0;
  auto a = make_robot(surface), b = make_robot(surface);
  for (int i = 0; i < 30; ++i) {
    a.step(.5f, .5f, .01f);
    b.step(.5f, .5f, .01f);
  }
  assert(a.x == b.x && a.y == b.y && a.yaw == b.yaw);
  assert(a.wl == b.wl && a.wr == b.wr);
  assert(a.force_left_n == 0 && std::fabs(a.force_right_n) > 0);
  assert(std::fabs(a.body_omega_rps) > 1.e-4f);
  assert(std::fabs(a.wl) > std::fabs(a.wr));
}
void lateral_motion_needs_ground_contact() {
  sim::Surface surface;
  surface.rolling_resistance_coefficient = 0;
  auto grounded = make_robot(surface);
  auto airborne_surface = surface;
  airborne_surface.contact_left_fraction = 0;
  airborne_surface.contact_right_fraction = 0;
  auto airborne = make_robot(airborne_surface);
  grounded.vy_m_s = airborne.vy_m_s = .2f;
  const float grounded_y = grounded.y, airborne_y = airborne.y;
  grounded.step(0, 0, .01f);
  airborne.step(0, 0, .01f);
  assert(std::fabs(grounded.vy_m_s) < .2f);
  assert(grounded.force_left_lat_n < 0 && grounded.force_right_lat_n < 0);
  assert(airborne.vy_m_s == .2f);
  assert(std::fabs((airborne.y - airborne_y) - .002f) < 1.e-6f);
  assert(std::fabs(grounded.y - grounded_y) < .002f);
}
void body_mass_changes_acceleration() {
  sim::Surface light;
  sim::Surface heavy = light;
  heavy.mass_kg = .5f;
  auto a = make_robot(light), b = make_robot(heavy);
  for (int i = 0; i < 20; ++i) {
    a.step(.3f, .3f, .001f);
    b.step(.3f, .3f, .001f);
  }
  assert(a.vx_m_s > b.vx_m_s + 1.e-4f);
}
void invalid_surface_params_are_rejected() {
  sim::Surface surface;
  surface.mu_kinetic = std::numeric_limits<float>::quiet_NaN();
  bool rejected = false;
  try {
    (void)make_robot(surface);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);
  surface = {};
  surface.contact_right_fraction = 1.01f;
  rejected = false;
  try {
    (void)make_robot(surface);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);
  surface = {};
  surface.mu_static = 6.f;
  rejected = false;
  try {
    (void)make_robot(surface);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);
}
} // namespace

int main() {
  no_motion_at_rest();
  rear_axle_contacts_couple_lateral_motion_and_yaw();
  legacy_pivot_preserves_rear_axle_center();
  absent_contact_only_spins_wheels();
  traction_is_friction_limited_and_slips();
  moderate_drive_sticks_without_extra_contact_force();
  coast_dissipates_without_reversal();
  unequal_contact_and_repeatability();
  lateral_motion_needs_ground_contact();
  body_mass_changes_acceleration();
  invalid_surface_params_are_rejected();
  std::cout << "surface dynamics tests passed\n";
}
