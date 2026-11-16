// SPDX-License-Identifier: GPL-3.0-only

#include "localization/config.hpp"
#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/end_cell.hpp"
#include "navigation/feature_tasks.hpp"
#include "navigation/map_resolver.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/navigation.hpp"
#include "navigation/path_executor.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
namespace {
nav::ResolverConfig config;
nav::RunDeadline deadline;
std::unique_ptr<nav::FeatureNavigation> features;
nav::FeatureSnapshot snapshot;
float x = .1f, y = .1f, yaw = 0, linear = 0, angular = 0;
bool fusion_started = false, fusion_stopped = false, halted = false,
     feature_halted = false, stale = false;
bool mismatch = false, driver_fault = false;
std::string scenario;
struct Log {
  robot_log::Level level;
  std::string message;
};
std::vector<Log> logs;
unsigned log_count(const char *text, robot_log::Level level) {
  unsigned count = 0;
  for (const auto &log : logs)
    if (log.level == level && log.message.find(text) != std::string::npos)
      ++count;
  return count;
}
nav::EndTapeDetector detector({150, 800});
nav::EndTapeEvent tape{};
uint32_t optical_sequence = 0;
int64_t optical_stamp = 0;
} // namespace
int64_t hal_time_us() { return fake::now; }
void hal_navigation_resolved(const loc::MazeMap &) {}
namespace robot_log {
bool write(Level level, const char *, const char *format, ...) {
  char message[96];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  logs.push_back({level, message});
  return true;
}
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
        trace_maze_ray(config.variants[0], mismatch ? .9f : x,
                       mismatch ? .9f : y, yaw, cfg::lidar_mounts[i])
            .range;
  }
  if (optical_stamp != t) {
    optical_stamp = t;
    const bool black =
        scenario == "bad_tape"
            ? false
            : (scenario == "turn" ? (x > .25f && y >= .17f && y <= .195f)
                                  : (x >= .17f && x <= .195f));
    loc::RgbcSample raw{};
    raw.sequence = ++optical_sequence;
    raw.clear = black ? 100 : 1200;
    detector.process(raw, t, &tape);
  }
  if (scenario == "bad_tape" && x > .11f && !tape.id)
    tape = {1, t, t};
  if (scenario != "no_tape")
    f->end_tape = tape;
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
  if (driver_fault)
    m->status = ControlStatus::DRIVER_FAULT;
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
  // Explicit open-arena fixture; production POIs belong to the supplied maze.
  config.start_count = 1;
  config.starts[0].cell = {0, 0};
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
  const bool test_fallback = scenario.find("fallback") == 0;
  bool saw_fallback = false, fallback_moved = false, cancel_sent = false,
       reissued = false, recovered = false, saw_fault = false;
  unsigned paused = 0, entries = 0;
  unsigned fault_frames = 0;
  bool fault_cancelled = false, fault_reissued = false, fault_recovered = false;
  bool was_fallback = false;
  int64_t end = 0;
  fake::run("path_exec", [&] {
    nav::NavigationSnapshot s{};
    assert(nav::navigation_get_state(&s));
    ++n;
    assert(s.engine == nav::NavigationEngine::FEATURE && !fusion_stopped);
    if (!test_fallback)
      assert(!s.fallback && !s.feature.contradiction_latched);
    if (s.startup == nav::StartupStatus::READY && !sent) {
      assert(fusion_started);
      if (scenario == "fallback_idle" && !s.feature.contradiction_latched) {
        mismatch = true;
        assert(linear == 0 && angular == 0 && !s.fallback &&
               s.run.state == nav::RunState::WAITING);
      } else {
        if (scenario == "fallback_idle")
          assert(!s.fallback && linear == 0 && angular == 0 &&
                 s.run.state == nav::RunState::WAITING &&
                 s.fallback_reason ==
                     nav::FallbackReason::FEATURE_CONTRADICTION);
        assert(nav::navigation_set_goal(scenario == "turn" ? loc::Cell{1, 1}
                                                           : loc::Cell{1, 0}));
        sent = true;
      }
    }
    if (s.run.state == nav::RunState::RUNNING) {
      if (!end)
        end = s.run.deadline_us;
      assert(end == s.run.deadline_us);
    }
    if (linear > 0)
      moved = true;
    if (test_fallback && moved && !saw_fallback)
      mismatch = true;
    if (test_fallback && s.fallback) {
      if (!was_fallback) {
        ++entries;
        if constexpr (nav::motion_test::rolling_turns) {
          assert(s.fallback_phase != nav::FallbackPhase::COAST);
          assert(std::isfinite(linear) && std::isfinite(angular));
        } else {
          assert(linear == 0 && angular == 0 &&
                 s.fallback_phase == nav::FallbackPhase::COAST);
        }
      }
      saw_fallback = true;
      assert(s.feature.contradiction_latched &&
             s.feature.contradiction_frames == 3 &&
             s.fallback_reason == nav::FallbackReason::FEATURE_CONTRADICTION);
      assert(log_count("FEATURE contradiction latched",
                       robot_log::Level::WARN) == 1);
      if (scenario == "fallback_tape") {
        // A cue without a matching independent pose must pause RHR, rather
        // than claim a goal from tape alone.
        tape = {1, fake::now, fake::now};
        if (s.end_tape_unresolved) {
          assert(linear == 0 && angular == 0 && !s.end_tape_seen);
          fake::now = end;
        }
      } else {
        mismatch = false;
        recovered = recovered || s.feature.pose.tracking;
        fallback_moved = fallback_moved || linear > 0 || angular != 0;
        // Continuous motion may precede FEATURE reacquisition. Exercise the
        // intended recovery/fault scenario only after both have occurred.
        if (fallback_moved && recovered) {
          if (scenario == "fallback_cancel" && !cancel_sent) {
            assert(nav::navigation_cancel());
            cancel_sent = true;
          } else if (scenario == "fallback_fault") {
            driver_fault = true;
            if (s.fallback_phase == nav::FallbackPhase::FAULT) {
              assert(linear == 0 && angular == 0 &&
                     log_count("-> FAULT", robot_log::Level::WARN) == 1);
              saw_fault = true;
              fake::now = end;
            }
          } else if (scenario == "fallback_stale") {
            if (s.fallback_phase == nav::FallbackPhase::PAUSED) {
              assert(linear == 0 && angular == 0 &&
                     log_count("-> PAUSED", robot_log::Level::WARN) == 1);
              if (++paused == 3)
                stale = false;
            } else if (!paused &&
                       s.fallback_phase == nav::FallbackPhase::FOLLOW)
              stale = true;
            else if (linear > 0 || angular != 0)
              fake::now = end;
          } else if (scenario == "fallback" || scenario == "fallback_idle" ||
                     reissued)
            fake::now = end;
        }
      }
    } else if (test_fallback && cancel_sent && !reissued && saw_fallback) {
      assert(linear == 0 && angular == 0 &&
             s.fallback_phase == nav::FallbackPhase::IDLE &&
             s.fallback_reason == nav::FallbackReason::FEATURE_CONTRADICTION);
      assert(nav::navigation_set_goal({1, 0}));
      reissued = true;
      fallback_moved = false;
    }
    was_fallback = s.fallback;
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
    if (scenario == "leg_timeout") {
      if (moved && !fault_frames)
        stale = true;
      if (!fault_cancelled &&
          s.execution == unsigned(nav::ExecuteStatus::FAULT)) {
        ++fault_frames;
        assert(linear == 0 && angular == 0 &&
               s.run.state == nav::RunState::RUNNING && fake::now < end &&
               s.execution_fault == unsigned(nav::ExecuteFault::LEG_TIMEOUT));
        assert(log_count("PATH FAULT reason=LEG_TIMEOUT",
                         robot_log::Level::ERROR) == 1);
        assert(log_count("pose tracking=0", robot_log::Level::ERROR) == 1);
        if (fault_frames == 3) {
          assert(nav::navigation_cancel());
          fault_cancelled = true;
        }
      } else if (fault_cancelled && !fault_reissued &&
                 s.execution == unsigned(nav::ExecuteStatus::IDLE)) {
        assert(s.execution_fault == unsigned(nav::ExecuteFault::NONE));
        stale = false;
        assert(nav::navigation_set_goal({1, 0}));
        fault_reissued = true;
      } else if (fault_reissued && linear > 0) {
        fault_recovered = true;
        fake::now = end;
      }
    }
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
      assert(s.startup == ((scenario == "goal" || scenario == "turn" ||
                            scenario == "fallback_goal")
                               ? nav::StartupStatus::DONE
                               : nav::StartupStatus::TIMEOUT));
      if (scenario == "stale")
        assert(saw_stale);
      if (scenario == "leg_timeout")
        assert(fault_frames == 3 && fault_cancelled && fault_reissued &&
               fault_recovered);
      if (test_fallback) {
        assert(saw_fallback &&
               s.fallback_reason == nav::FallbackReason::FEATURE_CONTRADICTION);
        assert(!s.fallback && s.fallback_phase == nav::FallbackPhase::IDLE);
        assert(log_count("right-hand fallback: FEATURE",
                         robot_log::Level::WARN) == entries);
        if (scenario != "fallback_tape")
          assert(recovered && fallback_moved);
        if (scenario == "fallback_cancel")
          assert(reissued && entries == 2);
        if (scenario == "fallback_fault")
          assert(saw_fault);
        if (scenario == "fallback_stale")
          assert(paused == 3);
      }
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
