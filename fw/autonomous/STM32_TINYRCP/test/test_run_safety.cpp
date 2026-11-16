// SPDX-License-Identifier: GPL-3.0-only

#include "localization/config.hpp"
#include "localization/fusion_engine.hpp"
#include "navigation/motion_experiment.hpp"
#include "navigation/right_hand_rule.hpp"
#include "navigation/run_deadline.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
namespace {
void deadlines() {
  nav::RunDeadline d;
  assert(d.poll(100000000).state == nav::RunState::WAITING);
  assert(d.arm(100000000));
  const auto start = d.poll(100000000);
  assert(start.deadline_us - start.started_us == 86000000);
  assert(d.arm(150000000));
  assert(d.poll(185999999).state == nav::RunState::RUNNING);
  d.goal(186000000);
  assert(d.poll(186000000).state == nav::RunState::TIMEOUT);
  assert(!d.arm(190000000));
  d.halt();
  assert(d.poll(0).state == nav::RunState::TIMEOUT);
  nav::RunDeadline g;
  assert(g.arm(0));
  g.goal(85999999);
  assert(g.poll(999999999).state == nav::RunState::GOAL && !g.arm(1000000000));
  nav::RunDeadline h;
  h.halt();
  assert(!h.arm(1));
  nav::RunDeadline back;
  assert(back.arm(100));
  assert(back.poll(99).state == nav::RunState::HALTED);
  nav::RunDeadline overflow;
  assert(!overflow.arm(INT64_MAX));
}
void health() {
  auto map = loc::MazeMap::q1();
  loc::FusionEngine engine;
  assert(engine.initialize(map, .5f, .5f, 0, 1000000));
  assert(engine.snapshot(1000000).health == loc::FusionHealth::INITIALIZING);
  assert(engine.snapshot(2000000).health == loc::FusionHealth::FAILED);
  for (int i = 1; i <= 3; ++i) {
    loc::SensorEvent e{};
    e.kind = loc::SensorKind::TOF;
    e.tof_id = loc::TofId::FRONT;
    e.timestamp_us = 1000000 + i * 10000;
    e.range_m = 1.2f;
    assert(!engine.process(e, e.timestamp_us));
    assert(engine.snapshot(e.timestamp_us).health ==
           (i < 3 ? loc::FusionHealth::INITIALIZING
                  : loc::FusionHealth::CONTRADICTING));
    // Replaying the same acquisition never increments contradiction evidence.
    assert(!engine.process(e, e.timestamp_us));
  }
  assert(!engine.snapshot(1030000).tracking);
  loc::FusionEngine invalid;
  assert(invalid.initialize(map, .5f, .5f, 0, 1000000));
  for (int i = 1; i <= 20; ++i) {
    loc::SensorEvent e{};
    e.kind = loc::SensorKind::TOF;
    e.timestamp_us = 1000000 + i * 10000;
    e.range_m = std::numeric_limits<float>::quiet_NaN();
    assert(!invalid.process(e, e.timestamp_us));
  }
  assert(invalid.snapshot(1200000).health != loc::FusionHealth::CONTRADICTING);
}
struct Rig {
  nav::BootstrapFrame f{};
  motor::MotorSnapshot m{};
  loc::PoseSnapshot pose{};
  nav::HeadingObservation heading{};
  int64_t now = 1000000;
  void step(int64_t us = 20000) {
    now += us;
    f.imu_us = m.timestamp_us = now;
    pose.timestamp_us = heading.timestamp_us = now;
    for (auto &t : f.tof_us)
      t = now;
  }
  Rig() {
    pose.tracking = pose.grid_confident = true;
    pose.grid_x = pose.grid_y = 0;
    pose.x_m = .1f + .002f;
    pose.y_m = .1f;
    heading.valid = true;
    f.range_m[0] = f.range_m[1] = .3f;
    f.range_m[2] = .04f;
    step();
  }
  nav::DriveRequest update(nav::RightHandRule &r) {
    return r.update(f, m, now, &pose, &heading);
  }
  void settle(nav::RightHandRule &r) {
    if constexpr (nav::motion_test::rolling_turns)
      return;
    for (int i = 0; i < 21; ++i) {
      step();
      auto d = update(r);
      assert(d.linear_m_s == 0 && d.angular_rad_s == 0);
    }
  }
};
void fallback() {
  Rig rig;
  nav::RightHandRule r;
  r.begin();
  rig.settle(r);
  rig.step();
  auto d = rig.update(r);
  assert(d.linear_m_s > 0 && d.angular_rad_s == 0);
  rig.f.range_m[2] = .02f;
  rig.step();
  d = rig.update(r);
  assert(d.angular_rad_s > 0); // Steer away from right wall.
  rig.f.range_m[2] = .4f;
  rig.step();
  d = rig.update(r);
  assert((d.linear_m_s > 0) == nav::motion_test::rolling_turns);
  // Choose right before front/left.
  rig.settle(r); // Center passage, fresh confirmations, then quiet coast.
  rig.f.imu.gz_rps = -.5f;
  bool turned = false;
  for (int i = 0; i < 160; ++i) {
    rig.step();
    d = rig.update(r);
    assert((d.linear_m_s > 0) == nav::motion_test::rolling_turns);
    if (r.phase() == nav::FallbackPhase::CLEAR) {
      turned = true;
      break;
    }
    assert(d.angular_rad_s < 0);
  }
  assert(turned);
  rig.f.imu.gz_rps = 0;
  for (int i = 0; i < 21; ++i) {
    rig.step();
    d = rig.update(r);
    assert(d.angular_rad_s == 0);
  }
  rig.step();
  d = rig.update(r);
  assert(d.linear_m_s > 0); // Committed exit suppresses repeated right turns.
  rig.f.tof_us[0] = rig.now - 200001;
  d = rig.update(r);
  assert(d.status == nav::ExecuteStatus::PAUSED && d.linear_m_s == 0 &&
         d.angular_rad_s == 0);
  for (bool left_open : {true, false}) {
    Rig q;
    nav::RightHandRule turn;
    turn.begin();
    q.settle(turn);
    q.f.range_m[1] = .100f - loc::cfg::lidar_mounts[1].x_body_m;
    q.f.range_m[0] = left_open ? .4f : .04f;
    q.step();
    d = q.update(turn);
    assert((d.linear_m_s > 0) == nav::motion_test::rolling_turns);
    q.settle(turn);
    q.step();
    d = q.update(turn);
    assert(d.angular_rad_s > 0); // Left or U-turn.
    q.f.imu_us = q.now - 100001;
    d = q.update(turn);
    assert(d.status == nav::ExecuteStatus::FAULT && d.angular_rad_s == 0);
  }
  Rig stuck;
  nav::RightHandRule turn;
  turn.begin();
  stuck.settle(turn);
  stuck.f.range_m[2] = .4f;
  stuck.step();
  stuck.update(turn);
  stuck.settle(turn);
  for (int i = 0; i < 401; ++i) {
    stuck.step();
    d = stuck.update(turn);
  }
  assert(d.status == nav::ExecuteStatus::FAULT && d.angular_rad_s == 0);
  Rig bad;
  nav::RightHandRule stop;
  stop.begin();
  bad.settle(stop);
  bad.f.range_m[1] = std::numeric_limits<float>::quiet_NaN();
  bad.step();
  d = bad.update(stop);
  assert(d.status == nav::ExecuteStatus::PAUSED && d.linear_m_s == 0);
}
void center_before_side_turn() {
  for (bool right : {false, true}) {
    Rig rig;
    nav::RightHandConfig cfg;
    cfg.front_stop_m = .060f; // Exercise left-branch centering before arrival.
    nav::RightHandRule controller(cfg);
    rig.pose.grid_x = 1;
    rig.pose.x_m = right ? .205f : .288f;
    rig.f.range_m[0] = right ? .033f : .4f;
    rig.f.range_m[2] = right ? .4f : .033f;
    rig.f.range_m[1] = right ? .5f : .112f - loc::cfg::lidar_mounts[1].x_body_m;
    controller.begin();
    rig.settle(controller);
    rig.step();
    auto drive = rig.update(controller);
    assert(controller.phase() == nav::FallbackPhase::CENTER);
    assert(drive.angular_rad_s == 0);
    // An opening and elapsed time alone cannot authorize the side pivot.
    for (unsigned i = 0; i < 25; ++i) {
      rig.step();
      drive = rig.update(controller);
      assert(controller.phase() == nav::FallbackPhase::CENTER &&
             drive.linear_m_s > 0 && drive.angular_rad_s == 0);
    }
    rig.pose.tracking = false;
    rig.heading.valid = false;
    rig.step();
    drive = rig.update(controller);
    assert(drive.status == nav::ExecuteStatus::PAUSED &&
           drive.linear_m_s == 0 && drive.angular_rad_s == 0);
    rig.pose.tracking = rig.heading.valid = true;
    rig.pose.x_m = .299f; // Safe diagonal clearance 1 mm before center.
    rig.f.range_m[1] = right ? .5f : .102f - loc::cfg::lidar_mounts[1].x_body_m;
    rig.step();
    drive = rig.update(controller);
    assert((drive.linear_m_s > 0) == nav::motion_test::rolling_turns);
    assert((drive.angular_rad_s != 0) == nav::motion_test::rolling_turns);
    for (unsigned i = 0;
         i < 30 && controller.phase() != nav::FallbackPhase::TURN; ++i) {
      rig.step();
      drive = rig.update(controller);
      assert(drive.linear_m_s == 0);
      if (controller.phase() != nav::FallbackPhase::TURN)
        assert(drive.angular_rad_s == 0);
    }
    rig.step();
    drive = rig.update(controller);
    assert(controller.phase() == nav::FallbackPhase::TURN &&
           ((drive.linear_m_s > 0) == nav::motion_test::rolling_turns) &&
           (right ? drive.angular_rad_s < 0 : drive.angular_rad_s > 0));
  }
}
} // namespace
int main() {
  deadlines();
  health();
  fallback();
  center_before_side_turn();
  std::puts("PASS absolute 86s deadline, goal precedence, EKF evidence, "
            "right/front/left/U-turn and sensor failures");
}
