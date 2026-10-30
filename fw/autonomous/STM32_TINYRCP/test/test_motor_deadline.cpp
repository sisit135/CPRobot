#include "localization/localization.hpp"
#include "motor_control/hal_motor.hpp"
#include "motor_control/motor_control.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <cstdio>
#include <string>
namespace {
float left = 0, right = 0;
bool loc_halted = false;
unsigned nonzero_writes = 0;
} // namespace
int64_t hal_time_us() { return fake::now; }
bool hal_motor_init() {
  left = right = 0;
  return true;
}
bool hal_motor_set_duty(float l, float r) {
  left = l;
  right = r;
  if (l != 0 || r != 0)
    ++nonzero_writes;
  return true;
}
void hal_motor_stop() { left = right = 0; }
namespace loc {
void localization_halt() { loc_halted = true; }
} // namespace loc
int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  assert(motor::motor_control_start());
  assert(motor::motor_stop());
  assert(motor::motor_run_state().state == nav::RunState::WAITING);
  // Exercise the real task/queue/HAL adapter without running a path task.
  assert(motor::motor_begin_run());
  const auto run = motor::motor_run_state();
  assert(motor::motor_set_twist(.03f, 0));
  unsigned iterations = 0;
  bool terminal = false;
  unsigned writes_at_stop = 0;
  fake::run("motor_pwm", [&] {
    motor::MotorSnapshot m{};
    assert(motor::motor_get_state(&m));
    ++iterations;
    if (!terminal && ((scenario == "timeout" && fake::now >= run.deadline_us) ||
                      (scenario != "timeout" && iterations == 4))) {
      if (scenario == "halt")
        motor::motor_halt();
      else if (scenario == "goal")
        motor::motor_goal_reached();
      terminal = true;
      writes_at_stop = nonzero_writes;
      assert(left == 0 && right == 0);
    }
    if (terminal) {
      assert(left == 0 && right == 0 && nonzero_writes == writes_at_stop);
      assert(!motor::motor_set_twist(.03f, 0));
      assert(!motor::motor_begin_run());
      if (scenario == "timeout") {
        assert(motor::motor_run_state().state == nav::RunState::TIMEOUT);
        assert(m.status == motor::ControlStatus::RUN_TIMEOUT && loc_halted);
      }
      if (iterations > (scenario == "timeout" ? 8603u : 7u))
        fake::finish();
    } else
      assert(motor::motor_set_twist(.03f, 0));
    fake::now += 10000;
  });
  assert(terminal && nonzero_writes > 0 && loc_halted);
  std::printf("PASS real motor task %s: latch, zero PWM and command rejection "
              "without path task\n",
              argv[1]);
}
