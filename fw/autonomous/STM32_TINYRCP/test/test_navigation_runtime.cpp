#include "localization/config.hpp"
#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/feature_tasks.hpp"
#include "navigation/map_resolver.hpp"
#include "navigation/navigation.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
namespace {
nav::RunDeadline deadline;
nav::ResolverConfig config;
bool failed = false, stopped_fusion = false, all_halted = false,
     goal_marker = false;
unsigned motor_stops = 0, twists = 0;
int64_t tape_time = 0;
int64_t goal_time = 0;
loc::FusionHealth injected = loc::FusionHealth::TRACKING;
} // namespace
int64_t hal_time_us() { return fake::now; }
void hal_navigation_resolved(const loc::MazeMap &) {}
namespace robot_log {
bool write(Level, const char *, const char *, ...) { return true; }
} // namespace robot_log
namespace loc {
bool localization_get_bootstrap(nav::BootstrapFrame *f) {
  *f = {};
  f->color = {800, 100, 100, 1200, unsigned(fake::now / 20000), false};
  f->color_us = f->imu_us = f->mag_us = fake::now;
  f->mag = {0, -45, 0};
  f->magnetic_clean = true;
  for (int i = 0; i < 3; ++i) {
    f->tof_us[i] = fake::now;
    f->range_m[i] = trace_maze_ray(config.variants[0], .1f, .1f, kPi / 2,
                                   cfg::lidar_mounts[i])
                        .range;
  }
  if (goal_marker) {
    if (!tape_time)
      tape_time = fake::now;
    f->end_tape = {1, tape_time, tape_time};
  }
  return true;
}
bool localization_start(const MazeMap &, const nav::Resolution &) {
  return true;
}
bool localization_ready() { return true; }
bool localization_get_pose(PoseSnapshot *p) {
  *p = {};
  p->timestamp_us = p->last_imu_us = fake::now;
  p->x_m = p->y_m = .1f;
  p->yaw_rad = kPi / 2;
  p->grid_x = p->grid_y = 0;
  p->tracking = !failed;
  p->health = injected;
  return true;
}
void localization_stop_fusion() { stopped_fusion = true; }
void localization_halt() { all_halted = true; }
} // namespace loc
namespace nav {
bool feature_task_start(const ResolverConfig &, const FeatureConfig &) {
  return true;
}
void feature_task_halt() {}
void feature_task_target(loc::Cell) {}
bool feature_task_snapshot(FeatureSnapshot *s) {
  *s = {};
  auto &p = s->pose;
  loc::localization_get_pose(&p);
  p.tracking = true;
  if (goal_marker)
    p.y_m = fake::now - tape_time < 100000 ? .19f : .3f;
  p.grid_y = int(p.y_m / .2f);
  s->heading = {loc::kPi / 2, fake::now, true};
  auto &r = s->resolution;
  r.status = ResolveStatus::RESOLVED;
  r.kind = loc::MapKind::Q1;
  r.cell = {0, 0};
  r.x_m = r.y_m = .1f;
  r.yaw_rad = loc::kPi / 2;
  r.heading_us = fake::now;
  return true;
}
} // namespace nav
namespace motor {
bool motor_control_start() { return true; }
bool motor_stop() {
  ++motor_stops;
  return true;
}
bool motor_set_twist(float, float) {
  if (deadline.poll(fake::now).terminal())
    return false;
  ++twists;
  return true;
}
bool motor_get_state(MotorSnapshot *m) {
  *m = {};
  m->timestamp_us = fake::now;
  return true;
}
bool motor_begin_run() { return deadline.arm(fake::now); }
nav::RunSnapshot motor_run_state() { return deadline.poll(fake::now); }
void motor_halt() { deadline.halt(); }
void motor_goal_reached() { deadline.goal(fake::now); }
} // namespace motor
int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  config = nav::default_resolver_config();
  config.starts[0] = {{0, 0}, nav::TileClass::BLUE_START, 15};
  config.start_count = 1;
  config.colors[0] = {.8f, .1f, .1f, nav::TileClass::BLUE_START};
  config.color_count = 1;
  config.colors_calibrated = true;
  assert(nav::navigation_start(config, nav::NavigationEngine::EKF));
  unsigned iterations = 0;
  bool sent = false, saw_fallback = false, canceled = false;
  int64_t original_deadline = 0;
  fake::run("path_exec", [&] {
    nav::NavigationSnapshot s{};
    assert(nav::navigation_get_state(&s));
    ++iterations;
    if (s.startup == nav::StartupStatus::READY && !sent) {
      assert(nav::navigation_set_goal({0, 1}));
      sent = true;
      goal_time = fake::now;
    }
    if (s.run.state == nav::RunState::RUNNING && !original_deadline)
      original_deadline = s.run.deadline_us;
    if (original_deadline)
      assert(s.run.deadline_us == original_deadline);
    if (sent && fake::now - goal_time > 100000 && !failed) {
      failed = true;
      injected = scenario == "failed" ? loc::FusionHealth::FAILED
                                      : loc::FusionHealth::CONTRADICTING;
    }
    if (s.fallback) {
      saw_fallback = true;
      assert(stopped_fusion);
    }
    if (saw_fallback && scenario == "cancel" && !canceled) {
      assert(nav::navigation_cancel());
      canceled = true;
    }
    if (saw_fallback && scenario == "goal" && fake::now - goal_time > 700000)
      goal_marker = true;
    if (saw_fallback &&
        (scenario == "timeout" || scenario == "failed" || canceled))
      fake::now = original_deadline;
    else
      fake::now += 20000;
    if (s.startup == nav::StartupStatus::TIMEOUT ||
        s.startup == nav::StartupStatus::DONE) {
      assert(sent && saw_fallback && all_halted && motor_stops > 0 &&
             twists > 0);
      assert(!nav::navigation_set_goal({1, 1}) && !nav::navigation_cancel());
      assert(s.startup == (scenario == "goal" ? nav::StartupStatus::DONE
                                              : nav::StartupStatus::TIMEOUT));
      fake::finish();
    }
    assert(iterations < 200);
  });
  std::printf("PASS real path task %s: bootstrap, EKF takeover, shared "
              "deadline, terminal rejection\n",
              argv[1]);
}
