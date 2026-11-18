// SPDX-License-Identifier: GPL-3.0-only
#include "lifecycle.hpp"
#include "bup/bup_tasks.hpp"
#include "hal/boot_sensor_sample.hpp"
#include "localization/config.hpp"
#include "localization/hal_sensors.hpp"
#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/hal_motor.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/motion_experiment.hpp"
#include "remote/settings.hpp"
#include "scheduler.hpp"
#include "start_validation.hpp"
#ifdef ROBOT_LIFECYCLE_LSM303DLHC
#include "hal/lsm303dlhc_hal.hpp"
#include "lsm303dlhc_device.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>
namespace lifecycle {
Session *active = nullptr;
const char *startup_name(nav::StartupStatus s) {
  static const char *names[] = {
      "RESOLVING", "INITIALIZING_EKF", "STARTING_MOTOR", "READY",
      "FAULT",     "TIMEOUT",          "HALTED",         "DONE"};
  return names[unsigned(s)];
}
const char *run_name(nav::RunState s) {
  switch (s) {
  case nav::RunState::WAITING:
    return "WAITING";
  case nav::RunState::RUNNING:
    return "RUNNING";
  case nav::RunState::GOAL:
    return "GOAL";
  case nav::RunState::TIMEOUT:
    return "TIMEOUT";
  case nav::RunState::HALTED:
    return "HALTED";
  }
  return "UNKNOWN";
}
const char *execution_name(unsigned s) {
  static const char *names[] = {"IDLE",   "TURN",     "ADVANCE", "SETTLE",
                                "PAUSED", "DONE",     "NO_PATH", "BLOCKED",
                                "FAULT",  "FALLBACK", "TIMEOUT", "HALTED"};
  return s < 12 ? names[s] : "UNKNOWN";
}
Session::Session(Options opt)
    : options(opt), maps(nav::default_resolver_config()) {
  if (active)
    throw std::runtime_error("one lifecycle per process; restart to reset");
  if (options.variant > 3 || !loc::valid_cell(options.goal) ||
      options.entrance < -1 || options.entrance >= int(maps.start_count) ||
      options.heading < -1 || options.heading > 3 ||
      options.range_override < -1 || options.range_override > 2 ||
      !std::isfinite(options.range_override_m) ||
      !std::isfinite(options.noise_sigma) || options.noise_sigma < 0 ||
      !std::isfinite(options.scrape_allowance_m) ||
      options.scrape_allowance_m < 0 || options.scrape_allowance_m > .005f ||
      (!nav::motion_test::enabled && options.scrape_allowance_m != 0))
    throw std::runtime_error("invalid scenario options");
  if (options.open_arena) {
    // Explicit integration fixture with a fully observable boundary rectangle.
    // The supplied Q1 map remains the default physical maze.
    auto m = maps.variants[0];
    for (int row = 0; row < loc::kMazeN; ++row)
      for (int col = 0; col < loc::kMazeN; ++col)
        m.walls[row][col] =
            (row == 0 ? loc::NORTH : 0) | (row == 4 ? loc::SOUTH : 0) |
            (col == 0 ? loc::WEST : 0) | (col == 4 ? loc::EAST : 0);
    for (unsigned v = 0; v < 4; ++v)
      maps.variants[v] = m.rotated(v, static_cast<loc::MapKind>(v));
  }
  std::mt19937 draw(options.seed);
  const auto drawn_entrance =
      std::uniform_int_distribution<unsigned>(0, maps.start_count - 1)(draw);
  const auto drawn_heading =
      std::uniform_int_distribution<unsigned>(0, 3)(draw);
  entrance = options.entrance < 0 ? drawn_entrance : unsigned(options.entrance);
  const auto &poi = maps.starts[entrance];
  start = loc::rotate_cell(poi.cell, options.variant);
  goal = loc::rotate_cell(options.goal, options.variant);
  const unsigned h =
      options.heading < 0 ? drawn_heading : unsigned(options.heading);
  initial_heading = (h + options.variant) % 4;

  const auto start_check =
      lifecycle::validate_start(maps.variants[options.variant], start, goal);
  route = start_check.path;
  const bool goal_route_required = !options.startup_only && !options.rhr_test;
  const bool invalid_scenario =
      start_check.reason == StartValidationReason::INVALID_MAP ||
      start_check.reason == StartValidationReason::INVALID_START ||
      start_check.reason == StartValidationReason::INVALID_GOAL;
  if (invalid_scenario || (goal_route_required && !start_check.reachable())) {
    std::fprintf(stderr,
                 "START_VALIDATION REJECTED reason=%s map=Q%u "
                 "start=(%d,%d) goal=(%d,%d) route_cells=%zu\n",
                 start_validation_reason(start_check.reason),
                 options.variant + 1, start.x, start.y, goal.x, goal.y,
                 route.size());
    throw std::runtime_error("scenario start cannot reach goal on known map");
  }

  sim::Noise noise;
  noise.range_sigma = options.noise_sigma;
  noise.gyro_bias = 0; // calibrated simulated IMU
  // The nominal observation is the center beam used by the firmware model.
  // An optional nearest-return fan exercises the unmodelled optical footprint.
  noise.tof_fov_deg = options.nearest_return ? 15 : 0;
  // Balanced reference wheel gains; first-order lag remains an assumption.
  robot = std::make_unique<sim::Robot>(
      maps.variants[options.variant], start, initial_heading * loc::kPi / 2,
      options.seed, noise,
      sim::Plant{motor::tt90::speed_per_duty_rad_s,
                 motor::tt90::speed_per_duty_rad_s, .15f, .15f},
      options.surface);
  rhr_test_initial_yaw = robot->yaw;
  config.set(200, options.variant_mask ? options.variant_mask
                                       : 1u << options.variant);
  config.set(240, 150);
  config.set(241, 800);
  remote::apply(config.store);
  active = this;
  std::printf("MOTION_PROFILE rolling_max=%u nonstop_turn=%u turn_guard=%u "
              "wheel_cap_rad_s=%.6f scrape_mm=%.3f\n",
              unsigned(nav::motion_test::enabled),
              unsigned(nav::motion_test::nonstop_only),
              unsigned(NAV_TURN_GUARD), double(motor::cfg::max_wheel_rad_s),
              double(options.scrape_allowance_m * 1000));
  robot_log::configure(options.quiet ? robot_log::Level::WARN
                                     : robot_log::Level::TRACE,
                       robot_log::UART);
  robot_log::set_sink(robot_log::UART, [](const robot_log::Record &r, void *) {
    std::printf("[%8.3f] %u %-15s %s\n", r.timestamp_ms / 1000.,
                unsigned(r.level), r.source, r.message);
    return true;
  });
  if (!bup::bup_task_start(loc::localization_sensors_start))
    throw std::runtime_error("boot failed");
  std::printf("SCENARIO seed=%u entrance=%u start=(%d,%d) yaw=%.3f map=Q%u "
              "end=(%d,%d) sigma=%.4f overlay_FOV=15 model=%s\n",
              options.seed, entrance, start.x, start.y, robot->yaw,
              options.variant + 1, goal.x, goal.y, options.noise_sigma,
              options.nearest_return ? "nearest" : "center");
  std::printf("arena=%s variant_mask=%u map_contradictions=%u route_cells=%u\n",
              options.open_arena ? "open" : "supplied",
              remote::feature_settings().variant_mask,
              maps.variants[options.variant].contradictions(), route.count);
  std::printf("draw entrance=%s heading=%s cardinal=%u end=%s\n",
              options.entrance < 0 ? "random" : "override",
              options.heading < 0 ? "random" : "override", initial_heading,
              options.goal == kEndGoal ? "fixed" : "override");
  std::printf(
      "GEOMETRY platform=%s width_mm=%.3f length_mm=%.3f "
      "front_tof_mount_mm=(%.3f,%.3f) track_mm=%.3f rear_envelope_mm=%.3f "
      "head=triangle head_base_y_mm=%.3f tip_y_mm=%.3f\n",
      robot::geometry::platform_v2 ? "v2" : "prototype",
      double(robot::geometry::platform_width_m * 1000),
      double(robot::geometry::platform_length_m * 1000),
      double(robot::geometry::front_tof_mount.x_m * 1000),
      double(robot::geometry::front_tof_mount.y_m * 1000),
      double(robot::geometry::track_width_m * 1000),
      double(robot::geometry::footprint_rear_m * 1000),
      double(robot::geometry::head_base_y_m * 1000),
      double(robot::geometry::head_points[1].y_m * 1000));
  const auto &surface = options.surface;
  std::printf("SURFACE model=%s mass_kg=%.3f mu_static=%.3f mu_kinetic=%.3f "
              "Crr=%.4f contact=(%.3f,%.3f)\n",
              surface.enabled ? "whiteboard" : "legacy",
              double(surface.mass_kg), double(surface.mu_static),
              double(surface.mu_kinetic),
              double(surface.rolling_resistance_coefficient),
              double(surface.contact_left_fraction),
              double(surface.contact_right_fraction));
}
double Session::seconds() const { return (now_us() - 1000000) / 1e6; }
void Session::halt() { nav::navigation_halt(); }
bool Session::body_collision(float inset_m) const {
  const float c = std::cos(robot->yaw), s = std::sin(robot->yaw);
  const auto &m = maps.variants[options.variant];
  // Clip independent physical wall segments against the body/tire envelope.
  auto intersects = [&](float ax, float ay, float bx, float by) {
    ax -= robot->x;
    ay -= robot->y;
    bx -= robot->x;
    by -= robot->y;
    const robot::geometry::BodyPoint a = {c * ax + s * ay, -s * ax + c * ay};
    const robot::geometry::BodyPoint b = {c * bx + s * by, -s * bx + c * by};
    return robot::geometry::footprint_intersects_segment(a, b, -inset_m);
  };
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x) {
      const auto mask = m.walls[4 - y][x];
      const float a = x * .2f, b = y * .2f;
      if (((mask & loc::NORTH) && intersects(a, b + .2f, a + .2f, b + .2f)) ||
          ((mask & loc::SOUTH) && intersects(a, b, a + .2f, b)) ||
          ((mask & loc::EAST) && intersects(a + .2f, b, a + .2f, b + .2f)) ||
          ((mask & loc::WEST) && intersects(a, b, a, b + .2f)))
        return true;
    }
  return false;
}
void Session::tick() {
  if (finished())
    return;
  robot->step(duty_left, duty_right, .001f);
  const bool contact = body_collision();
  if (contact)
    ++scrape_ticks_;
  robot->collided = robot->collided ||
                    (contact && body_collision(options.scrape_allowance_m));
  advance_clock(1000);
  run_tasks();
  robot_log::drain(32);
  loc::localization_get_bootstrap(&sensors);
  loc::localization_get_pose(&observer);
  motor::motor_get_state(&motor);
  const float applied =
      std::max(std::fabs(motor.duty_left), std::fabs(motor.duty_right));
  peak_pwm_ = std::max(peak_pwm_, applied);
  if (applied > 0) {
    ++moving_ticks_;
    if (applied >= 1.f - 1e-5f)
      ++full_scale_ticks_;
  }
  const auto now = now_us();
  if (!ready_us && bup::bup_task_get_status() == bup::BupStatus::SUCCEEDED &&
      sensors.imu_us) {
    ready_us = now;
    robot_log::write(robot_log::Level::INFO, "lifecycle",
                     "board READY; loc_start in 5 seconds");
  }
  if (!key_sent && ready_us && now - ready_us >= 5000000) {
    key_sent = true;
    key_us_ = now;
    robot_log::write(robot_log::Level::INFO, "loc_start",
                     options.force_rhr
                         ? "simulated key: start FEATURE with forced RHR"
                         : "simulated key: start FEATURE");
    if (!nav::navigation_start(maps, nav::NavigationEngine::FEATURE,
                               remote::feature_settings(),
                               remote::fallback_settings()))
      result = "FAIL: loc_start rejected";
  }
  if (key_sent && nav::navigation_get_state(&navigation)) {
    forced_rhr_seen_ |=
        navigation.fallback &&
        navigation.fallback_reason == nav::FallbackReason::FORCED_TEST;
    if (!observed_terminal_us_ && navigation.run.terminal())
      observed_terminal_us_ = now;
    if (int(navigation.startup) != previous_startup ||
        navigation.execution != previous_execution) {
      robot_log::write(robot_log::Level::INFO, "lifecycle", "%s / %s / %s",
                       startup_name(navigation.startup),
                       execution_name(navigation.execution),
                       run_name(navigation.run.state));
      previous_startup = int(navigation.startup);
      previous_execution = navigation.execution;
    }
    if (options.rhr_test) {
      if (navigation.fallback &&
          navigation.fallback_reason == nav::FallbackReason::FORCED_TEST &&
          !rhr_test_saw_forced) {
        rhr_test_saw_forced = true;
        rhr_test_entered_untracked = !navigation.feature.pose.tracking;
        rhr_test_before_contradiction =
            !navigation.feature.contradiction_latched &&
            navigation.feature.contradiction_frames == 0;
      }
      rhr_test_saw_coast |=
          navigation.fallback_phase == nav::FallbackPhase::COAST;
      rhr_test_saw_decide |=
          navigation.fallback_phase == nav::FallbackPhase::DECIDE;
      rhr_test_saw_turn |=
          navigation.fallback_phase == nav::FallbackPhase::TURN;
      rhr_test_saw_clear |=
          navigation.fallback_phase == nav::FallbackPhase::CLEAR;
      rhr_test_saw_follow |=
          navigation.fallback_phase == nav::FallbackPhase::FOLLOW;
      if (navigation.fallback_phase == nav::FallbackPhase::FOLLOW) {
        if (!rhr_test_follow_since_us)
          rhr_test_follow_since_us = now;
      } else
        rhr_test_follow_since_us = 0;
      rhr_test_saw_motion |=
          std::fabs(duty_left) > .001f || std::fabs(duty_right) > .001f;
      const float travelled =
          std::hypot(robot->x - (start.x + .5f) * loc::kCellM,
                     robot->y - (start.y + .5f) * loc::kCellM);
      const float turned =
          std::fabs(loc::wrap_pi(robot->yaw - rhr_test_initial_yaw));
      if (rhr_test_saw_forced && rhr_test_saw_coast && rhr_test_saw_decide &&
          rhr_test_saw_turn && rhr_test_saw_clear && rhr_test_saw_follow &&
          rhr_test_saw_motion && rhr_test_follow_since_us &&
          now - rhr_test_follow_since_us >= 1000000 && travelled >= .05f &&
          turned >= 1.1f && !rhr_test_halt_sent && !robot->collided) {
        rhr_test_success_halt = true;
        rhr_test_halt_sent = true;
        robot_log::write(robot_log::Level::INFO, "lifecycle",
                         "forced RHR E2E: phase progression observed; halting");
        nav::navigation_halt();
      }
      if (!rhr_test_halt_sent && seconds() >= 35.0) {
        rhr_test_halt_sent = true;
        robot_log::write(robot_log::Level::ERROR, "lifecycle",
                         "forced RHR E2E: phase/motion watchdog; halting");
        nav::navigation_halt();
      }
    }
    if (options.startup_only &&
        navigation.startup == nav::StartupStatus::READY) {
      std::printf(
          "STARTUP_RESULT status=%s resolver=%u kind=%u cell=(%d,%d) "
          "candidates=%u ranges=(%.4f,%.4f,%.4f) heading=%.4f "
          "goal_sent=%u duties=(%.3f,%.3f) tracking=%u\n",
          startup_name(navigation.startup),
          unsigned(navigation.feature.resolution.status),
          unsigned(navigation.feature.resolution.kind),
          navigation.feature.resolution.cell.x,
          navigation.feature.resolution.cell.y, navigation.feature.candidates,
          sensors.range_m[0], sensors.range_m[1], sensors.range_m[2],
          navigation.feature.heading.yaw_rad, unsigned(goal_sent), duty_left,
          duty_right, unsigned(navigation.feature.pose.tracking));
      result = "STARTUP REPORT";
    } else if (options.startup_only && seconds() >= 12.0) {
      std::printf(
          "STARTUP_RESULT status=%s resolver=%u kind=%u cell=(%d,%d) "
          "candidates=%u ranges=(%.4f,%.4f,%.4f) heading=%.4f "
          "goal_sent=%u duties=(%.3f,%.3f) tracking=%u\n",
          startup_name(navigation.startup),
          unsigned(navigation.feature.resolution.status),
          unsigned(navigation.feature.resolution.kind),
          navigation.feature.resolution.cell.x,
          navigation.feature.resolution.cell.y, navigation.feature.candidates,
          sensors.range_m[0], sensors.range_m[1], sensors.range_m[2],
          navigation.feature.heading.yaw_rad, unsigned(goal_sent), duty_left,
          duty_right, unsigned(navigation.feature.pose.tracking));
      result = "STARTUP REPORT";
    }
    if (!options.startup_only && !goal_sent &&
        navigation.startup == nav::StartupStatus::READY) {
      goal_sent = nav::navigation_set_goal(goal);
      if (!goal_sent)
        result = "FAIL: end goal rejected";
    }
    if (navigation.startup == nav::StartupStatus::FAULT)
      result = "FAIL: navigation startup fault";
    if (options.rhr_test && rhr_test_halt_sent &&
        navigation.run.state == nav::RunState::HALTED &&
        navigation.terminal_reported) {
      if (!terminal_us)
        terminal_us = now;
      if (now - terminal_us >= 500000) {
        const bool safe_stop = !robot->collided && duty_left == 0 &&
                               duty_right == 0 && std::fabs(robot->wl) < .05f &&
                               std::fabs(robot->wr) < .05f;
        const float travelled =
            std::hypot(robot->x - (start.x + .5f) * loc::kCellM,
                       robot->y - (start.y + .5f) * loc::kCellM);
        const float turned =
            std::fabs(loc::wrap_pi(robot->yaw - rhr_test_initial_yaw));
        rhr_test_deadline_preserved =
            navigation.run.started_us > 0 && navigation.run.deadline_us > now &&
            navigation.run.deadline_us - navigation.run.started_us ==
                nav::RunDeadline::kBudgetUs;
        passed = rhr_test_saw_forced && rhr_test_entered_untracked &&
                 rhr_test_before_contradiction && rhr_test_saw_coast &&
                 rhr_test_saw_decide && rhr_test_saw_turn &&
                 rhr_test_saw_clear && rhr_test_saw_follow &&
                 rhr_test_saw_motion && rhr_test_success_halt && safe_stop &&
                 travelled >= .05f && turned >= 1.1f &&
                 rhr_test_deadline_preserved;
        std::printf(
            "RHR_TEST_RESULT reason=%u forced=%u untracked=%u pre_contra=%u "
            "coast=%u decide=%u turn=%u clear=%u follow=%u motion=%u "
            "travelled=%.3f turned=%.3f deadline=%u halted=%u zero_pwm=%u "
            "collision=%u tracked=%u\n",
            unsigned(navigation.fallback_reason), unsigned(rhr_test_saw_forced),
            unsigned(rhr_test_entered_untracked),
            unsigned(rhr_test_before_contradiction),
            unsigned(rhr_test_saw_coast), unsigned(rhr_test_saw_decide),
            unsigned(rhr_test_saw_turn), unsigned(rhr_test_saw_clear),
            unsigned(rhr_test_saw_follow), unsigned(rhr_test_saw_motion),
            travelled, turned, unsigned(rhr_test_deadline_preserved),
            unsigned(navigation.run.state == nav::RunState::HALTED),
            unsigned(duty_left == 0 && duty_right == 0),
            unsigned(robot->collided),
            unsigned(navigation.feature.pose.tracking));
        result = passed ? "PASS: forced RHR halted safely"
                        : "FAIL: forced RHR lifecycle checks";
      }
    } else if (navigation.run.terminal() && navigation.terminal_reported) {
      if (!terminal_us)
        terminal_us = now;
      // Continue physics and terminal tasks for 500 ms to verify stopping.
      if (now - terminal_us >= 500000) {
        const float error = std::hypot(robot->x - (goal.x + .5f) * loc::kCellM,
                                       robot->y - (goal.y + .5f) * loc::kCellM);
        passed = navigation.run.state == nav::RunState::GOAL &&
                 navigation.startup == nav::StartupStatus::DONE &&
                 motor.status == motor::ControlStatus::GOAL_REACHED &&
                 navigation.end_tape_seen && error <= .025f &&
                 !robot->collided && duty_left == 0 && duty_right == 0 &&
                 std::fabs(robot->wl) < .05f && std::fabs(robot->wr) < .05f;
        result = passed
                     ? "PASS: end cell reached AND loc/run GOAL; motors stopped"
                     : "FAIL: terminal state or physical end-cell check";
      }
    }
  }
  if (robot->collided) {
    halt();
    result = "FAIL: physical wall collision";
  }
  if (seconds() > 110) {
    halt();
    result = "FAIL: lifecycle watchdog (startup/run incomplete)";
  }
  if (!key_sent && (duty_left != 0 || duty_right != 0))
    result = "FAIL: motion before loc_start";
  if (now - last_trail >= 50000) {
    trail.push_back({robot->x, robot->y});
    last_trail = now;
  }
  if (finished()) {
    robot_log::drain(32);
    if (options.quiet) {
      const float error = std::hypot(robot->x - (goal.x + .5f) * loc::kCellM,
                                     robot->y - (goal.y + .5f) * loc::kCellM);
      std::printf(
          "METRICS key_boot_s=%.3f terminal_boot_s=%.3f goal_from_key_s=%.3f "
          "end_error_m=%.6f tape=%u collision=%u zero_pwm=%u stopped=%u "
          "motor_goal=%u forced_rhr=%u run_budget_us=%lld\n",
          key_us_ ? (key_us_ - 1000000) / 1e6 : -1.,
          observed_terminal_us_ ? (observed_terminal_us_ - 1000000) / 1e6 : -1.,
          navigation.run.state == nav::RunState::GOAL && key_us_
              ? (observed_terminal_us_ - key_us_) / 1e6
              : -1.,
          double(error), unsigned(navigation.end_tape_seen),
          unsigned(robot->collided),
          unsigned(duty_left == 0 && duty_right == 0),
          unsigned(std::fabs(robot->wl) < .05f && std::fabs(robot->wr) < .05f),
          unsigned(motor.status == motor::ControlStatus::GOAL_REACHED),
          unsigned(forced_rhr_seen_),
          static_cast<long long>(navigation.run.deadline_us -
                                 navigation.run.started_us));
    }
    std::printf("MOTION_METRICS peak_pwm=%.6f moving_ms=%llu "
                "full_scale_ms=%llu scrape_ms=%llu\n",
                double(peak_pwm_),
                static_cast<unsigned long long>(moving_ticks_),
                static_cast<unsigned long long>(full_scale_ticks_),
                static_cast<unsigned long long>(scrape_ticks_));
    std::printf("%s t=%.3f pose=(%.4f,%.4f) state=%s seed=%u\n", result.c_str(),
                seconds(), robot->x, robot->y, run_name(navigation.run.state),
                options.seed);
    std::fflush(stdout);
  }
}
} // namespace lifecycle
int64_t hal_time_us() { return lifecycle::now_us(); }
// The lifecycle observes sensor-task snapshots; the boot display is exercised
// separately by robot_boot_sensor_sample. Keep its BUP hooks linkable here.
namespace boot_sensors {
void capture(Snapshot &out) { out = {}; }
void print(const Snapshot &) {}
} // namespace boot_sensors
#ifdef ROBOT_LIFECYCLE_TEST
namespace nav {
bool lifecycle_force_rhr() {
  return lifecycle::active && lifecycle::active->options.force_rhr;
}
bool lifecycle_startup_only() {
  return lifecycle::active && lifecycle::active->options.startup_only;
}
} // namespace nav
#endif
void hal_log_pose(const loc::PoseSnapshot &) {}
void hal_navigation_resolved(const loc::MazeMap &) {}
bool hal_motor_init() { return true; }
namespace remote {
bool board_init() {
  return true;
} // Simulation settings were applied before boot.
} // namespace remote
bool hal_motor_set_duty(float l, float r) {
  lifecycle::active->duty_left = l;
  lifecycle::active->duty_right = r;
  return true;
}
void hal_motor_stop() { hal_motor_set_duty(0, 0); }
#ifdef ROBOT_LIFECYCLE_LSM303DLHC
namespace {
void update_lsm_device();
}
bool hal_sensors_init() {
  lsm_fake::device.reset();
  lsm_fake::device.before_transfer = update_lsm_device;
  imu::Config config;
  config.i2c = &lsm_fake::device.bus;
  std::puts("SENSOR_BACKEND LSM303DLHC raw registers -> ST BSP/HAL -> MAG+ACCEL; no gyro");
  return imu::initialize(config);
}
#else
bool hal_sensors_init() { return true; }
#endif
namespace {
nav::BootstrapFrame sample() {
  static int64_t stamp = 0;
  static nav::BootstrapFrame f;
  if (stamp != hal_time_us()) {
    stamp = hal_time_us();
    f = lifecycle::active->robot->sensors(stamp, 1);
#ifdef ROBOT_LIFECYCLE_LSM303DLHC
    // The HAL receives only independent ACCEL/MAG samples, quantized to the
    // configured silicon formats. The simulator's gyro never enters this path.
    const float a[] = {f.imu.ax_mps2, f.imu.ay_mps2, f.imu.az_mps2};
    const float m[] = {f.mag.mx_uT, f.mag.my_uT, f.mag.mz_uT};
    lsm_fake::device.sample(stamp, a, m);
#endif
    const auto &opt = lifecycle::active->options;
    if (opt.range_override >= 0)
      f.range_m[opt.range_override] = opt.range_override_m;
  }
  return f;
}
#ifdef ROBOT_LIFECYCLE_LSM303DLHC
void update_lsm_device() { (void)sample(); }
#endif
} // namespace
#ifndef ROBOT_LIFECYCLE_LSM303DLHC
bool hal_imu_read(loc::ImuSample *v, int64_t *t) {
  auto f = sample();
  *v = f.imu;
  *t = f.imu_us;
  return true;
}
bool hal_mag_read(loc::MagSample *v, int64_t *t) {
  auto f = sample();
  *v = f.mag;
  *t = f.mag_us;
  return true;
}
#endif
bool hal_magnetic_clean() {
#ifdef ROBOT_LIFECYCLE_LSM303DLHC
  return lsm303dlhc::magnetic_heading_usable();
#else
  return sample().magnetic_clean;
#endif
}
bool hal_tof_red_distance_corrected(loc::TofId id, float *v, int64_t *t) {
  auto f = sample();
  *v = f.range_m[unsigned(id)];
  *t = f.tof_us[unsigned(id)];
  return true;
}
bool hal_tcs34725_read(loc::RgbcSample *v, int64_t *t) {
  static int64_t last = 0;
  static uint32_t sequence = 0;
  if (hal_time_us() - last < 24000)
    return false; // distinct 24 ms integrations
  last = hal_time_us();
  auto &s = *lifecycle::active;
  auto &p = *s.robot;
  const float x = p.x + std::cos(p.yaw) * loc::cfg::color_x_body_m -
                  std::sin(p.yaw) * loc::cfg::color_y_body_m;
  const float y = p.y + std::sin(p.yaw) * loc::cfg::color_x_body_m +
                  std::cos(p.yaw) * loc::cfg::color_y_body_m;
  bool black = false;
  for (auto d :
       {loc::Cell{1, 0}, loc::Cell{-1, 0}, loc::Cell{0, 1}, loc::Cell{0, -1}}) {
    loc::Cell from{s.goal.x - d.x, s.goal.y - d.y};
    if (!s.maps.variants[s.options.variant].can_step(from, s.goal))
      continue;
    const float dx = x - ((s.goal.x + .5f) * .2f - d.x * .1f);
    const float dy = y - ((s.goal.y + .5f) * .2f - d.y * .1f);
    const float along = dx * d.x + dy * d.y, lateral = -dx * d.y + dy * d.x;
    black |= along >= -.025f && along <= -.005f && std::fabs(lateral) < .08f;
  }
  *v = {1000,       1000,
        1000,       uint16_t(black && !s.options.no_tape ? 100 : 1200),
        ++sequence, false};
  *t = last;
  return true;
}
bool hal_end_tape_classify(const loc::RgbcSample &v, int64_t t,
                           nav::EndTapeEvent *out) {
  static nav::EndTapeDetector detector([] {
    uint16_t black = 0, white = 0;
    remote::tape_thresholds(&black, &white);
    return nav::TapeConfig{black, white};
  }());
  return detector.process(v, t, out);
}
