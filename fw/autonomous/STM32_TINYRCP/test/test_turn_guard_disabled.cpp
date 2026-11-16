// SPDX-License-Identifier: GPL-3.0-only
#include "navigation/turn_guard.hpp"
#include <cassert>
int main() {
  static_assert(NAV_TURN_GUARD == 0);
  nav::TurnGuard guard;
  nav::DriveRequest request{};
  request.angular_rad_s = .5f;
  const auto out = guard.update({}, {}, {}, {}, request, 1000000);
  assert(out.angular_rad_s == request.angular_rad_s && !guard.busy());
}
