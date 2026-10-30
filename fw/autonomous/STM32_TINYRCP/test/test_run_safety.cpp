#include "localization/config.hpp"
#include "localization/fusion_engine.hpp"
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
  int64_t now = 1000000;
  void step(int64_t us = 20000) {
    now += us;
    f.imu_us = m.timestamp_us = now;
    for (auto &t : f.tof_us)
      t = now;
  }
  Rig() {
    f.range_m[0] = f.range_m[1] = .3f;
    f.range_m[2] = .04f;
    step();
  }
  void settle(nav::RightHandRule &r) {
    for (int i = 0; i < 21; ++i) {
      step();
      auto d = r.update(f, m, now);
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
  auto d = r.update(rig.f, rig.m, rig.now);
  assert(d.linear_m_s > 0 && d.angular_rad_s == 0);
  rig.f.range_m[2] = .02f;
  rig.step();
  d = r.update(rig.f, rig.m, rig.now);
  assert(d.angular_rad_s > 0); // Steer away from right wall.
  rig.f.range_m[2] = .4f;
  rig.step();
  d = r.update(rig.f, rig.m, rig.now);
  assert(d.linear_m_s == 0); // Choose right before front/left.
  rig.f.imu.gz_rps = -.5f;
  bool turned = false;
  for (int i = 0; i < 160; ++i) {
    rig.step();
    d = r.update(rig.f, rig.m, rig.now);
    assert(d.linear_m_s == 0);
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
    d = r.update(rig.f, rig.m, rig.now);
    assert(d.angular_rad_s == 0);
  }
  rig.step();
  d = r.update(rig.f, rig.m, rig.now);
  assert(d.linear_m_s > 0); // Committed exit suppresses repeated right turns.
  rig.f.tof_us[0] = rig.now - 200001;
  d = r.update(rig.f, rig.m, rig.now);
  assert(d.status == nav::ExecuteStatus::PAUSED && d.linear_m_s == 0 &&
         d.angular_rad_s == 0);
  for (bool left_open : {true, false}) {
    Rig q;
    nav::RightHandRule turn;
    turn.begin();
    q.settle(turn);
    q.f.range_m[1] = .03f;
    q.f.range_m[0] = left_open ? .4f : .04f;
    q.step();
    d = turn.update(q.f, q.m, q.now);
    assert(d.linear_m_s == 0);
    q.step();
    d = turn.update(q.f, q.m, q.now);
    assert(d.angular_rad_s > 0); // Left or U-turn.
    q.f.imu_us = q.now - 100001;
    d = turn.update(q.f, q.m, q.now);
    assert(d.status == nav::ExecuteStatus::FAULT && d.angular_rad_s == 0);
  }
  Rig stuck;
  nav::RightHandRule turn;
  turn.begin();
  stuck.settle(turn);
  stuck.f.range_m[2] = .4f;
  stuck.step();
  turn.update(stuck.f, stuck.m, stuck.now);
  for (int i = 0; i < 401; ++i) {
    stuck.step();
    d = turn.update(stuck.f, stuck.m, stuck.now);
  }
  assert(d.status == nav::ExecuteStatus::FAULT && d.angular_rad_s == 0);
  Rig bad;
  nav::RightHandRule stop;
  stop.begin();
  bad.settle(stop);
  bad.f.range_m[1] = std::numeric_limits<float>::quiet_NaN();
  bad.step();
  d = stop.update(bad.f, bad.m, bad.now);
  assert(d.status == nav::ExecuteStatus::PAUSED && d.linear_m_s == 0);
}
} // namespace
int main() {
  deadlines();
  health();
  fallback();
  std::puts("PASS absolute 86s deadline, goal precedence, EKF evidence, "
            "right/front/left/U-turn and sensor failures");
}
