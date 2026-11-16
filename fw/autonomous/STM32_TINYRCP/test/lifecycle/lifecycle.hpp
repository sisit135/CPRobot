// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "AStar.hpp"
#include "memory_store.hpp"
#include "navigation/navigation.hpp"
#include "robot_sim.hpp"
#include <memory>
#include <string>
#include <vector>
namespace lifecycle {
inline constexpr loc::Cell kEndGoal{0, 0};
struct Options {
  uint32_t seed = 0;
  int entrance = -1;
  int heading = -1; // Q1 cardinal 0=E, 1=N, 2=W, 3=S; -1 draws randomly.
  unsigned variant = 0;
  unsigned variant_mask =
      0; // Zero: configured arena variant; 15: blind search.
  loc::Cell goal = kEndGoal; // Overrides are for focused diagnostic fixtures.
  float noise_sigma = .002f;
  sim::Surface surface{true}; // Interactive default: rubber on a smooth board.
  bool nearest_return = false;
  bool open_arena = false;
  bool no_tape = false;
  bool startup_only = false;
  bool force_rhr = false;
  bool rhr_test = false;
  bool quiet =
      false; // Host-only log filtering; plant/task timing is unchanged.
  float scrape_allowance_m =
      0; // Host overlap tolerance, not wall-contact physics.
  int range_override = -1;
  float range_override_m = 0;
};
struct Point {
  float x, y;
};
class Session {
public:
  explicit Session(Options options);
  void tick(); // one deterministic 1 ms plant/scheduler step
  void halt();
  double seconds() const;
  bool finished() const { return !result.empty(); }
  bool passed = false, key_sent = false, goal_sent = false;
  std::string result;
  Options options;
  unsigned entrance = 0;
  unsigned initial_heading = 0;
  loc::Cell start{}, goal{};
  nav::ResolverConfig maps;
  std::unique_ptr<sim::Robot> robot;
  nav::NavigationSnapshot navigation{};
  nav::BootstrapFrame sensors{};
  motor::MotorSnapshot motor{};
  loc::PoseSnapshot observer{};
  AStar::CoordinateList route;
  std::vector<Point> trail;
  float duty_left = 0, duty_right = 0;
  int64_t ready_us = 0;
  MemoryStore config;
  bool rhr_test_saw_forced = false, rhr_test_saw_coast = false,
       rhr_test_saw_decide = false, rhr_test_saw_turn = false,
       rhr_test_saw_clear = false, rhr_test_saw_follow = false,
       rhr_test_saw_motion = false, rhr_test_halt_sent = false,
       rhr_test_entered_untracked = false,
       rhr_test_before_contradiction = false,
       rhr_test_deadline_preserved = false, rhr_test_success_halt = false;
  int64_t rhr_test_follow_since_us = 0;
  float rhr_test_initial_yaw = 0;

private:
  bool body_collision(float inset_m = 0) const;
  uint64_t moving_ticks_ = 0, full_scale_ticks_ = 0, scrape_ticks_ = 0;
  float peak_pwm_ = 0;
  int64_t last_trail = 0, terminal_us = 0;
  int64_t key_us_ = 0, observed_terminal_us_ = 0;
  bool forced_rhr_seen_ = false;
  int previous_startup = -1, previous_execution = -1;
};
extern Session *active;
const char *startup_name(nav::StartupStatus);
const char *run_name(nav::RunState);
const char *execution_name(unsigned);
} // namespace lifecycle
