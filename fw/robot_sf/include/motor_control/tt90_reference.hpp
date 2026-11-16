// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Provisional TT 1:90 / DRV8833 reference, not measured board calibration.
// Source values, equations and limitations: docs/tt90_drive_model.md.
namespace motor::tt90 {
constexpr float supply_v = 6.f, robot_mass_kg = .250f;
constexpr float gear_ratio = 90.f, wheel_radius_m = .0325f;
constexpr float reference_voltage_v = 6.f, reference_no_load_rpm = 120.f;
// Current/speed: Adafruit 3801/3802; torque: MakerBotics MB-ELC-TTMOTOR-MG90.
// These are comparable motors, not confirmation of the installed motor SKU.
constexpr float no_load_current_a = .100f, reference_stall_current_a = 1.f;
constexpr float reference_stall_torque_nm = 1.5f * .0980665f;
// Typical HS+LS resistance, not a fixed voltage drop.
constexpr float bridge_resistance_ohm = .360f;
constexpr float bridge_peak_current_a = 2.f; // Per bridge; not a motor rating.
constexpr float reference_no_load_rad_s =
    reference_no_load_rpm * (2.f * 3.14159265358979323846f / 60.f);
constexpr float winding_resistance_ohm =
    reference_voltage_v / reference_stall_current_a;
constexpr float total_resistance_ohm =
    winding_resistance_ohm + bridge_resistance_ohm;
// Constants referred to the geared OUTPUT shaft. Do not multiply torque or
// wheel speed by the 90:1 ratio again for the directly attached wheel.
constexpr float back_emf_v_per_rad_s =
    (reference_voltage_v - no_load_current_a * winding_resistance_ohm) /
    reference_no_load_rad_s;
constexpr float torque_nm_per_a =
    reference_stall_torque_nm / (reference_stall_current_a - no_load_current_a);
constexpr float speed_per_duty_rad_s = supply_v / back_emf_v_per_rad_s;
// Constant no-load friction approximation. Real breakaway duty must be
// measured.
constexpr float running_duty_offset =
    no_load_current_a * total_resistance_ohm / supply_v;
constexpr float duty_per_rad_s = back_emf_v_per_rad_s / supply_v;

constexpr float no_load_speed_rad_s(float duty) {
  return duty > running_duty_offset
             ? (duty - running_duty_offset) * speed_per_duty_rad_s
             : 0.f;
}
constexpr float standstill_current_a(float duty) {
  const float current = duty * supply_v / total_resistance_ohm;
  return current < bridge_peak_current_a ? current : bridge_peak_current_a;
}
constexpr float standstill_torque_nm(float duty) {
  const float current = standstill_current_a(duty);
  return current > no_load_current_a
             ? torque_nm_per_a * (current - no_load_current_a)
             : 0.f;
}
// Two motors; straight-line upper bound ignoring tire slip, wheel/rotor
// inertia and rolling losses. Actual acceleration cannot be inferred from
// mass and gear ratio alone.
constexpr float torque_accel_ceiling_m_s2(float duty) {
  return 2.f * standstill_torque_nm(duty) / (wheel_radius_m * robot_mass_kg);
}
static_assert(reference_stall_current_a > no_load_current_a &&
                  back_emf_v_per_rad_s > 0 && robot_mass_kg > 0,
              "Invalid motor reference");
} // namespace motor::tt90
