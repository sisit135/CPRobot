// SPDX-License-Identifier: GPL-3.0-only

#include "localization/fusion_engine.hpp"
#include "motor_control/config.hpp"
#include "motor_control/wheel_controller.hpp"
#include "navigation/map_resolver.hpp"
#include "navigation/path_executor.hpp"
#include "robot_sim.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
namespace {
void feed(loc::FusionEngine &engine, const nav::BootstrapFrame &f, int64_t now,
          bool ranges = true) {
  loc::SensorEvent e{};
  e.kind = loc::SensorKind::IMU;
  e.timestamp_us = f.imu_us;
  e.imu = f.imu;
  engine.process(e, now);
  e = {};
  e.kind = loc::SensorKind::MAG;
  e.timestamp_us = f.mag_us;
  e.mag = f.mag;
  e.magnetic_clean = f.magnetic_clean;
  engine.process(e, now);
  if (ranges)
    for (int i = 0; i < 3; ++i) {
      e = {};
      e.kind = loc::SensorKind::TOF;
      e.timestamp_us = f.tof_us[i];
      e.tof_id = static_cast<loc::TofId>(i);
      e.range_m = f.range_m[i];
      engine.process(e, now);
    }
}
void pipeline(unsigned variant, uint32_t seed, bool inject_outage,
              bool multi_goal = false, sim::Plant dynamics = {},
              bool expect_completion = true) {
  auto c = nav::default_resolver_config();
  c.starts[0] = {{0, 0}, nav::TileClass::BLUE_START, 15};
  c.start_count = 1;
  c.colors[0] = {.8f, .1f, .1f, nav::TileClass::BLUE_START};
  c.color_count = 1;
  c.colors_calibrated = true;
  const auto start = loc::rotate_cell({0, 0}, variant),
             goal = loc::rotate_cell(
                 multi_goal ? loc::Cell{1, 2} : loc::Cell{0, 1}, variant);
  sim::Robot plant(c.variants[variant], start,
                   loc::kPi / 2 + variant * loc::kPi / 2, seed, {}, dynamics);
  nav::MapResolver resolver(c);
  nav::PathExecutor executor;
  std::unique_ptr<loc::FusionEngine> engine;
  std::unique_ptr<motor::WheelController> motor;
  motor::MotorSnapshot output{};
  nav::BootstrapFrame held_ranges{};
  bool commanded = false, paused = false, completed = false,
       failures_checked = false, outage_paused = false, contained = false;
  int64_t begin_us = 0;
  for (int k = 0; k < 6000; ++k) {
    const int64_t now = 1000000 + int64_t(k) * 10000;
    auto f = plant.sensors(now, k + 1);
    const bool outage = inject_outage && commanded &&
                        now - begin_us > 1500000 && now - begin_us < 2200000;
    if (k % 5 == 0 && !outage)
      held_ranges = f;
    else
      for (int i = 0; i < 3; ++i) {
        f.tof_us[i] = held_ranges.tof_us[i];
        f.range_m[i] = held_ranges.range_m[i];
      }
    if (!engine) {
      if (k % 5 == 0) {
        auto resolved = resolver.update(f, now);
        if (resolved.status == nav::ResolveStatus::RESOLVED) {
          assert(unsigned(resolved.kind) == variant);
          assert(!motor);
          engine = std::make_unique<loc::FusionEngine>();
          assert(engine->initialize(resolver.map(resolved.kind), resolved.x_m,
                                    resolved.y_m, resolved.yaw_rad,
                                    resolved.heading_us));
        }
      }
      assert(!motor); // No motor controller exists during unresolved startup.
    }
    if (engine) {
      feed(*engine, f, now,
           !outage && k % 5 == 0); // IMU every 10 ms, ToF every 50 ms.
      auto p = engine->snapshot(now);
      if (!motor && p.tracking) {
        motor = std::make_unique<motor::WheelController>();
        assert(engine);
      }
      if (motor) {
        if (!commanded && p.tracking) {
          assert(executor.begin(
              resolver.map(static_cast<loc::MapKind>(variant)), goal, p, now));
          commanded = true;
          begin_us = now;
        }
        auto drive = executor.update(p, output, f, now);
        if (drive.status == nav::ExecuteStatus::PAUSED) {
          assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
          paused = true;
          if (outage)
            outage_paused = true;
        }
        if (!failures_checked && drive.status == nav::ExecuteStatus::ADVANCE &&
            drive.linear_m_s > 0) {
          failures_checked = true;
          auto check = executor;
          auto stale = p;
          stale.tracking = false;
          auto request = check.update(stale, output, f, now);
          assert(request.status == nav::ExecuteStatus::PAUSED &&
                 request.linear_m_s == 0 && request.angular_rad_s == 0);
          check = executor;
          auto old = f;
          old.tof_us[1] = now - 100001;
          request = check.update(p, output, old, now);
          assert(request.status == nav::ExecuteStatus::PAUSED &&
                 request.linear_m_s == 0);
          check = executor;
          old = f;
          old.range_m[1] = .02f;
          request = check.update(p, output, old, now);
          assert(request.status == nav::ExecuteStatus::BLOCKED &&
                 request.linear_m_s == 0);
          for (auto fault : {motor::ControlStatus::TIMING_FAULT,
                             motor::ControlStatus::INVALID_COMMAND,
                             motor::ControlStatus::DRIVER_FAULT,
                             motor::ControlStatus::COMMAND_TIMEOUT}) {
            check = executor;
            auto bad = output;
            bad.status = fault;
            request = check.update(p, bad, f, now);
            assert(request.status == nav::ExecuteStatus::FAULT &&
                   request.linear_m_s == 0 && request.angular_rad_s == 0);
          }
        }
        if (!expect_completion && drive.status == nav::ExecuteStatus::FAULT) {
          std::printf("E2E mismatch containment=%s tracking=%u\n",
                      nav::execute_fault_name(drive.fault),
                      unsigned(p.tracking));
          assert(drive.linear_m_s == 0 && drive.angular_rad_s == 0);
          // The center-passage guard can expose an off-route tracked cell
          // before the excessive motor mismatch degrades EKF tracking.
          assert(
              (drive.fault == nav::ExecuteFault::UNEXPECTED_CELL &&
               p.tracking) ||
              (drive.fault == nav::ExecuteFault::LEG_TIMEOUT && !p.tracking));
          output = motor->update({0, 0, now}, now);
          assert(output.duty_left == 0 && output.duty_right == 0 &&
                 !plant.collided);
          contained = true;
          break; // Excess mismatch is contained before further actuation.
        }
        assert(drive.status != nav::ExecuteStatus::FAULT &&
               drive.status != nav::ExecuteStatus::BLOCKED &&
               drive.status != nav::ExecuteStatus::NO_PATH);
        float l = (drive.linear_m_s -
                   drive.angular_rad_s * motor::cfg::track_width_m / 2) /
                  motor::cfg::wheel_radius_m;
        float r = (drive.linear_m_s +
                   drive.angular_rad_s * motor::cfg::track_width_m / 2) /
                  motor::cfg::wheel_radius_m;
        output = motor->update(
            {l, r, now}, now); // No ground-truth wheel speed reaches control.
        if (drive.status == nav::ExecuteStatus::DONE) {
          completed = true;
          assert(std::hypot(plant.x - (goal.x + .5f) * .2f,
                            plant.y - (goal.y + .5f) * .2f) < .045f);
          break;
        }
      }
    }
    plant.step(output.duty_left, output.duty_right, .01f);
    assert(!plant.collided);
  }
  if (!expect_completion) {
    assert(contained && !completed && failures_checked && paused);
    std::puts("PASS E2E excess motor mismatch: off-route cell or tracking loss "
              "faults with zero PWM");
    return;
  }
  if (!completed)
    std::printf("E2E incomplete variant=%u commanded=%d xy=%g,%g paused=%d\n",
                variant, commanded, plant.x, plant.y, paused);
  assert(completed && failures_checked);
  if (inject_outage)
    assert(outage_paused);
  std::printf("PASS E2E variant=%u seed=%u outage=%d gains=(%.1f,%.1f) "
              "final=(%.4f,%.4f)\n",
              variant, seed, inject_outage, dynamics.left_gain,
              dynamics.right_gain, plant.x, plant.y);
  // A dropped IMU stream must invalidate tracking and prevent a new route.
  auto p = engine->snapshot(70000000);
  assert(!p.tracking);
  assert(executor.begin(c.variants[variant], start, engine->snapshot(1000000),
                        70000000) == false);
}
} // namespace
int main() {
  for (unsigned k = 0; k < 4; ++k)
    pipeline(k, 1234 + k, false);
  pipeline(0, 2026, true);
  pipeline(0, 4321, false, true);
  // Shifted side observations keep the historical 17/21 plant observable.
  // Retain every negative containment assertion and increase the asymmetry.
  // Failed left drive and an over-responsive right drive force containment;
  // the original fault, tracking-loss and zero-PWM assertions remain intact.
  pipeline(0, 5678, false, true, {0, 30, .18f, .14f}, false);
}
