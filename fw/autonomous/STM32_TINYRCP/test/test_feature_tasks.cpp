#include "localization/config.hpp"
#include "localization/localization.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/feature_tasks.hpp"
#include "navigation/map_resolver.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
namespace {
nav::ResolverConfig config;
nav::RunDeadline deadline;
float position = .1f;
bool stale = false;
} // namespace
int64_t hal_time_us() { return fake::now; }
namespace motor {
nav::RunSnapshot motor_run_state() { return deadline.poll(fake::now); }
} // namespace motor
namespace loc {
bool localization_get_bootstrap(nav::BootstrapFrame *f) {
  *f = {};
  const int64_t t = stale ? fake::now - 200000 : fake::now;
  f->imu_us = f->mag_us = t;
  f->mag = {45, 0, 0};
  f->magnetic_clean = true;
  for (int i = 0; i < 3; ++i) {
    f->tof_us[i] = t;
    f->range_m[i] = trace_maze_ray(config.variants[0], position, .1f, 0,
                                   cfg::lidar_mounts[i])
                        .range;
  }
  return true;
}
} // namespace loc
int main() {
  config = nav::default_resolver_config();
  auto &m = config.variants[0];
  for (int r = 0; r < 5; ++r)
    for (int c = 0; c < 5; ++c)
      m.walls[r][c] = (r == 0 ? loc::NORTH : 0) | (r == 4 ? loc::SOUTH : 0) |
                      (c == 0 ? loc::WEST : 0) | (c == 4 ? loc::EAST : 0);
  nav::FeatureConfig settings;
  settings.variant_mask = 1;
  assert(nav::feature_task_start(config, settings));
  nav::feature_task_target({1, 0});
  unsigned iterations = 0;
  bool observed_stop = false, observed_stale = false;
  fake::run("feature_nav", [&] {
    nav::FeatureSnapshot s{};
    assert(nav::feature_task_snapshot(&s));
    ++iterations;
    if (iterations > 3 && iterations < 45)
      position = std::min(.3f, position + .005f);
    if (iterations == 46) {
      assert(s.pose.tracking && s.stop.valid && s.stop.should_stop);
      observed_stop = true;
      stale = true;
    }
    if (iterations == 47) {
      assert(!s.pose.tracking && !s.stop.valid);
      observed_stale = true;
      deadline.arm(fake::now);
      fake::now += nav::RunDeadline::kBudgetUs;
    } else
      fake::now += 50000;
    assert(iterations < 49);
  });
  nav::FeatureSnapshot s{};
  assert(nav::feature_task_snapshot(&s));
  assert(observed_stop && observed_stale && !s.pose.tracking && !s.stop.valid);
  std::puts("PASS actual feature task: HAL snapshot input, target mailbox, "
            "measured stop, stale, terminal quiescence");
}
