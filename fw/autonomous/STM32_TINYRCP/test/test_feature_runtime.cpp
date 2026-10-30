#include "localization/config.hpp"
#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/end_cell.hpp"
#include "navigation/feature_tasks.hpp"
#include "navigation/map_resolver.hpp"
#include "navigation/navigation.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
namespace {
nav::ResolverConfig config;
nav::RunDeadline deadline;
std::unique_ptr<nav::FeatureNavigation> features;
nav::FeatureSnapshot snapshot;
float x = .1f, y = .1f, yaw = 0, linear = 0, angular = 0;
bool fusion_started = false, fusion_stopped = false, halted = false,
     feature_halted = false, stale = false;
std::string scenario;
nav::EndTapeDetector detector({150, 800});
nav::EndTapeEvent tape{};
uint32_t optical_sequence = 0;
int64_t optical_stamp = 0;
} // namespace
int64_t hal_time_us() { return fake::now; }
void hal_navigation_resolved(const loc::MazeMap &) {}
namespace robot_log {
bool write(Level, const char *, const char *, ...) { return true; }
} // namespace robot_log
namespace loc {
bool localization_get_bootstrap(nav::BootstrapFrame *f) {
  *f = {};
  auto t = stale ? fake::now - 200000 : fake::now;
  f->imu_us = f->mag_us = t;
  f->mag = {45 * std::cos(yaw), -45 * std::sin(yaw), 0};
  f->imu.gz_rps = angular;
  f->magnetic_clean = linear == 0 && angular == 0;
  for (int i = 0; i < 3; ++i) {
    f->tof_us[i] = t;
    f->range_m[i] =
        trace_maze_ray(config.variants[0], x, y, yaw, cfg::lidar_mounts[i])
            .range;
  }
  if (optical_stamp != t) {
    optical_stamp = t;
    const bool black = scenario == "turn"
                           ? (x > .25f && y >= .17f && y <= .195f)
                           : (x >= .17f && x <= .195f);
    loc::RgbcSample raw{};
    raw.sequence = ++optical_sequence;
    raw.clear = black ? 100 : 1200;
    detector.process(raw, t, &tape);
  }
  if (scenario != "no_tape")
    f->end_tape = tape;
  if (scenario == "bad_tape" && x > .11f && !tape.id)
    tape = {1, t, t};
  return true;
}
bool localization_start(const MazeMap &, const nav::Resolution &) {
  fusion_started = true;
  return true;
}
bool localization_ready() {
  return false;
} // Never block feature mode on EKF readiness.
bool localization_get_pose(PoseSnapshot *p) {
  *p = {};
  p->health = FusionHealth::FAILED;
  return true;
}
void localization_stop_fusion() { fusion_stopped = true; }
void localization_halt() { halted = true; }
} // namespace loc
namespace nav {
// Scheduler-independent bridge: actual feature engine, real path task. The
// real feature task's mailbox/termination behavior has its own fixture.
bool feature_task_start(const ResolverConfig &c, const FeatureConfig &f) {
  features = std::make_unique<FeatureNavigation>(c, f);
  return true;
}
void feature_task_halt() { feature_halted = true; }
void feature_task_target(loc::Cell c) { features->set_stop_target(c); }
bool feature_task_snapshot(FeatureSnapshot *s) {
  BootstrapFrame f;
  loc::localization_get_bootstrap(&f);
  snapshot = features->update(f, fake::now);
  *s = snapshot;
  return true;
}
} // namespace nav
namespace motor {
bool motor_control_start() { return true; }
bool motor_stop() {
  linear = angular = 0;
  return true;
}
bool motor_set_twist(float v, float w) {
  if (deadline.poll(fake::now).terminal())
    return false;
  linear = v;
  angular = w;
  return true;
}
bool motor_get_state(MotorSnapshot *m) {
  *m = {};
  m->timestamp_us = fake::now;
  m->duty_left = m->duty_right = (linear > 0 || angular != 0) ? .3f : 0;
  return true;
}
bool motor_begin_run() { return deadline.arm(fake::now); }
nav::RunSnapshot motor_run_state() {
  auto s = deadline.poll(fake::now);
  if (s.terminal())
    linear = 0;
  return s;
}
void motor_halt() {
  deadline.halt();
  linear = 0;
}
void motor_goal_reached() {
  deadline.goal(fake::now);
  linear = 0;
}
} // namespace motor
int main(int argc, char **argv) {
  assert(argc == 2);
  scenario = argv[1];
  config = nav::default_resolver_config();
  auto &m = config.variants[0];
  for (int r = 0; r < 5; ++r)
    for (int c = 0; c < 5; ++c)
      m.walls[r][c] = (r == 0 ? loc::NORTH : 0) | (r == 4 ? loc::SOUTH : 0) |
                      (c == 0 ? loc::WEST : 0) | (c == 4 ? loc::EAST : 0);
  // Identical map copies force full default mask to remain unresolved. Replace
  // other variants by supplied Q maps: observed long rays select the arena.
  assert(nav::navigation_start(config));
  unsigned n = 0;
  bool sent = false, moved = false, saw_stale = false;
  int64_t end = 0;
  fake::run("path_exec", [&] {
    nav::NavigationSnapshot s{};
    assert(nav::navigation_get_state(&s));
    ++n;
    assert(s.engine == nav::NavigationEngine::FEATURE && !s.fallback &&
           !fusion_stopped);
    if (s.startup == nav::StartupStatus::READY && !sent) {
      assert(fusion_started);
      assert(nav::navigation_set_goal(scenario == "turn" ? loc::Cell{1, 1}
                                                         : loc::Cell{1, 0}));
      sent = true;
    }
    if (s.run.state == nav::RunState::RUNNING) {
      if (!end)
        end = s.run.deadline_us;
      assert(end == s.run.deadline_us);
    }
    if (linear > 0)
      moved = true;
    if (s.end_tape_seen) {
      assert(s.startup != nav::StartupStatus::DONE ||
             std::hypot(x - .3f, y - (scenario == "turn" ? .3f : .1f)) <=
                 .012f);
    }
    if (scenario == "bad_tape" && s.end_tape_unresolved) {
      assert(linear == 0 && !s.end_tape_seen);
      fake::now = end;
    }
    if (scenario == "no_tape" && sent && x > .289f && linear == 0) {
      assert(s.startup != nav::StartupStatus::DONE && !s.end_tape_seen);
      fake::now = end;
    }
    if (moved && scenario == "timeout")
      fake::now = end;
    if (moved && scenario == "stale" && !stale)
      stale = true;
    else if (stale && scenario == "stale") {
      assert(linear == 0 && !s.feature.pose.tracking);
      saw_stale = true;
      fake::now = end;
    }
    if (s.terminal_reported && (s.startup == nav::StartupStatus::DONE ||
                                s.startup == nav::StartupStatus::TIMEOUT)) {
      assert(sent && moved && halted && feature_halted && linear == 0);
      assert(s.startup == ((scenario == "goal" || scenario == "turn")
                               ? nav::StartupStatus::DONE
                               : nav::StartupStatus::TIMEOUT));
      if (scenario == "stale")
        assert(saw_stale);
      assert(!nav::navigation_set_goal({2, 0}));
      fake::finish();
    }
    // Synthetic plant, not wheel feedback. Estimator only sees ray ranges.
    x += linear * std::cos(yaw) * .05f;
    y += linear * std::sin(yaw) * .05f;
    yaw = loc::wrap_pi(yaw + angular * .05f);
    fake::now += 50000;
    assert(n < 1000);
  });
  std::printf("PASS default feature runtime %s: independent of failed EKF, "
              "ray-driven path, shared deadline\n",
              argv[1]);
}
