#pragma once
#include <cstdint>
#include <limits>
namespace nav {
enum class RunState : uint8_t { WAITING, RUNNING, GOAL, TIMEOUT, HALTED };
struct RunSnapshot {
  RunState state = RunState::WAITING;
  int64_t started_us = 0, deadline_us = 0;
  bool terminal() const {
    return state == RunState::GOAL || state == RunState::TIMEOUT ||
           state == RunState::HALTED;
  }
};
/** One absolute budget per boot. Caller serializes all access (including 64-bit
 * fields). */
class RunDeadline {
public:
  static constexpr int64_t kBudgetUs = 86000000;
  // Start immediately before the first navigation motor command, even a zero
  // twist. Repeated goals, pauses, cancels and mode changes cannot extend this
  // budget.
  bool arm(int64_t now) {
    poll(now);
    if (s_.state == RunState::WAITING) {
      if (now < 0 || now > std::numeric_limits<int64_t>::max() - kBudgetUs) {
        halt();
        return false;
      }
      s_ = {RunState::RUNNING, now, now + kBudgetUs};
    }
    return s_.state == RunState::RUNNING;
  }
  RunSnapshot poll(int64_t now) {
    if (s_.state == RunState::RUNNING) {
      if (now < s_.started_us)
        s_.state = RunState::HALTED;
      else if (now >= s_.deadline_us)
        s_.state = RunState::TIMEOUT;
    }
    return s_;
  }
  void goal(int64_t now) {
    if (poll(now).state == RunState::RUNNING)
      s_.state = RunState::GOAL;
  }
  void halt() {
    if (!s_.terminal())
      s_.state = RunState::HALTED;
  }

private:
  RunSnapshot s_{};
};
} // namespace nav
