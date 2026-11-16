// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "motor_control/tt90_reference.hpp"
#include "motor_control/types.hpp"
#include "navigation/types.hpp"
#include "platform_geometry.hpp"
#include <random>
#include <stdexcept>
namespace sim {
struct Noise {
  float gyro_sigma = .002f, gyro_bias = .003f, range_sigma = .002f,
        mag_sigma = .001f;
  float tof_fov_deg = 0; // Zero preserves the existing pencil-ray fixtures.
};
struct Plant {
  // Affine duty-to-output-speed slopes above a provisional friction offset.
  // The right-side mismatch and lag constants remain simulation assumptions.
  float left_gain = motor::tt90::speed_per_duty_rad_s,
        right_gain = .95f * motor::tt90::speed_per_duty_rad_s,
        left_tau_s = .15f, right_tau_s = .17f;
  float left_duty_offset = motor::tt90::running_duty_offset,
        right_duty_offset = motor::tt90::running_duty_offset;
};
/** @brief Optional rigid-body/contact model for surface-aware simulation.
 *
 * Friction values are scenario assumptions, not measurements of the installed
 * TT90 wheels on a whiteboard. Rubber/glass studies show strong dependence on
 * compound, speed, and contamination (Tiwari et al., Frontiers in Mechanical
 * Engineering, 2021, doi:10.3389/fmech.2020.620233). The default coefficients
 * are a plausible dry-rubber proxy; use measured values for a specific board.
 * Rolling resistance 0.02 is illustrative: Blickle reports 54 N for 500 kg
 * on 200 mm elastic solid rubber 65 Shore A wheels (Crr≈0.011), versus 152 N
 * for 80 Shore A solid rubber (Crr≈0.031), under its laboratory trolley
 * conditions. Those wheels are unlike this robot's tires, so the reference
 * only bounds an example scale; https://www.blickle.com/-/media/project/
 * blickle/global-web/service/mediathek/prospekte/pdf/en/
 * fibel_ziehen_und_schieben_en.pdf. Contact fractions scale available normal
 * load per wheel; they are not contact-area percentages.
 */
struct Surface {
  bool enabled = false;
  float mass_kg = .250f;
  float chassis_diagonal_m = robot::geometry::platform_diagonal_m;
  float mu_static = .9f;
  float mu_kinetic = .7f;
  float rolling_resistance_coefficient = .02f;
  float contact_left_fraction = 1.f;
  float contact_right_fraction = 1.f;
  // Assumed equivalent output inertia: 10 g wheel rim plus a 20e-9 kg·m²
  // geared rotor reflected through 90:1. Both values are parameters because
  // the installed wheel/gearbox inertias are not measured.
  float wheel_rim_mass_kg = .010f;
  float motor_rotor_inertia_kg_m2 = 2.e-8f;
};
/** @brief Independent differential-drive plant and simulated sensor library.
 * @note Uses std::mt19937 with a caller seed; no runtime dependency downloads.
 * Ground-truth range uses grid DDA, independently of production segment
 * tracing.
 */
class Robot {
public:
  /** @brief Construct a stationary plant in a map cell.
   * @param[in] map Physical maze for simulation.
   * @param[in] cell Initial cell.
   * @param[in] yaw Initial world heading in radians.
   * @param[in] seed Random seed, fixed by each test.
   * @param[in] noise Sensor noise parameters in SI units.
   * @param[in] plant Independent PWM-to-speed plant parameters, hidden from
   * control.
   * @returns No value (constructor).
   */
  Robot(loc::MazeMap map, loc::Cell cell, float yaw, uint32_t seed,
        Noise noise = {}, Plant plant = {}, Surface surface = {});
  /** @brief Integrate the selected wheel/surface plant from signed PWM.
   * @param[in] left Left duty [-1,+1].
   * @param[in] right Right duty [-1,+1].
   * @param[in] dt Step in seconds.
   * @returns No value; collision is recorded in collided.
   */
  void step(float left, float right, float dt);
  /** @brief Generate fresh IMU, MAG, TCS and three ToF frames.
   * @param[in] now Shared acquisition time in microseconds.
   * @param[in] sequence Fresh optical sequence number.
   * @returns Snapshot with seeded Gaussian noise and physical ray distances.
   */
  nav::BootstrapFrame sensors(int64_t now, uint32_t sequence);
  // Independent physical DDA, also used to draw the sampled optical fan.
  float ray(const loc::LidarMount &mount) const { return range(mount); }
  // Ground truth only; the controller and executor never receive wl/wr.
  float x, y, yaw, wl = 0, wr = 0;
  bool collided = false;
  // Surface-mode ground truth. Body velocity is in world axes; wheel slip is
  // wheel tread speed minus chassis velocity at that wheel's contact point.
  float vx_m_s = 0, vy_m_s = 0, body_omega_rps = 0;
  float slip_left_m_s = 0, slip_right_m_s = 0;
  // Longitudinal/lateral tire force on the body (rolling drag is excluded).
  float force_left_n = 0, force_right_n = 0;
  float force_left_lat_n = 0, force_right_lat_n = 0;

private:
  float range(const loc::LidarMount &mount) const;
  float gaussian(float sigma);
  loc::MazeMap map_;
  Noise noise_;
  Plant plant_;
  Surface surface_;
  std::mt19937 rng_;
  std::normal_distribution<float> normal_{0, 1};
  float omega_ = 0;
  float body_inertia_kg_m2_ = 0;
  float wheel_inertia_kg_m2_ = 0;
  void step_surface(float left, float right, float dt);
};
} // namespace sim
