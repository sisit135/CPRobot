// SPDX-License-Identifier: GPL-3.0-only
#include "motor_control/config.hpp"
#include "motor_control/wheel_controller.hpp"
#include "navigation/turn_guard.hpp"
#include "robot_sim.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <string>

namespace {
// Independent v2 corner oracle, including the rear tire overhang. Keep the
// numeric drawing dimensions here rather than reusing the guard's slab test.
struct Bounds { float min_x, max_x, min_y, max_y; };
Bounds bounds(float x, float y, float yaw, float padding = 0) {
  Bounds out{INFINITY, -INFINITY, INFINITY, -INFINITY};
  for (float bx : {-.0565f - padding, .055f + padding})
    for (float by : {-.0675f - padding, .0675f + padding}) {
      const float px = x + bx * std::cos(yaw) - by * std::sin(yaw);
      const float py = y + bx * std::sin(yaw) + by * std::cos(yaw);
      out.min_x = std::min(out.min_x, px); out.max_x = std::max(out.max_x, px);
      out.min_y = std::min(out.min_y, py); out.max_y = std::max(out.max_y, py);
    }
  return out;
}
loc::MazeMap corridor() {
  auto map = loc::MazeMap::q1();
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x)
      map.walls[4 - y][x] = (y == 0 ? loc::SOUTH : 0) |
                            (y == 4 ? loc::NORTH : 0) | loc::EAST | loc::WEST;
  return map;
}
loc::PoseSnapshot pose(const sim::Robot &robot, int64_t now) {
  loc::PoseSnapshot p{};
  p.x_m = robot.x;
  p.y_m = robot.y;
  p.yaw_rad = robot.yaw;
  p.grid_x = int(p.x_m / loc::kCellM);
  p.grid_y = int(p.y_m / loc::kCellM);
  p.timestamp_us = now;
  p.tracking = true;
  return p;
}
nav::DriveRequest turn() {
  nav::DriveRequest d{};
  d.status = nav::ExecuteStatus::TURN;
  d.angular_rad_s = -.5f;
  d.diagnostics.decision = nav::ExecuteDecision::TURN_TO_HEADING;
  d.diagnostics.pivot_angle_rad = -loc::kPi / 2;
  d.diagnostics.pivot_angle_valid = true;
  return d;
}
void recovery(bool glide, bool right) {
  const auto map = corridor();
  sim::Noise noise{};
  noise.gyro_sigma = noise.gyro_bias = noise.mag_sigma = 0;
  noise.range_sigma = .00025f;
  sim::Surface surface;
  surface.enabled = true;
  sim::Robot robot(map, {2, 2}, loc::kPi / 2, 71, noise, {}, surface);
  const float sign = right ? 1.f : -1.f;
  robot.x = glide ? .5f + sign * .002f : (right ? .508f : .500f);
  robot.vx_m_s = glide ? sign * .3f : 0;
  nav::TurnGuard guard;
  motor::WheelController wheels;
  motor::MotorSnapshot motor{};
  int64_t now = 1000000;
  bool reverse = false, forward = false, corrected = false, unsafe = false;
  nav::TurnGuardPhase previous = nav::TurnGuardPhase::IDLE;
  for (int i = 0; i < 1600; ++i) {
    now += 10000;
    auto f = robot.sensors(now, i + 1);
    motor.timestamp_us = now;
    const auto request =
        guard.update(map, pose(robot, now), f, motor, turn(), now);
    if (guard.phase() != previous) {
      std::printf("t=%.2f %s -> %s x=%.5f y=%.5f yaw=%.3f\n", i * .01,
                  nav::turn_guard_phase_name(previous),
                  nav::turn_guard_phase_name(guard.phase()), robot.x, robot.y,
                  robot.yaw);
      previous = guard.phase();
    }
    assert(guard.phase() != nav::TurnGuardPhase::BLOCKED);
    unsafe = unsafe || std::min(robot.x - .4f, .6f - robot.x) < .095f;
    reverse = reverse || request.linear_m_s < 0;
    forward = forward || request.linear_m_s > 0;
    if (guard.phase() == nav::TurnGuardPhase::COAST)
      assert(request.linear_m_s == 0 && request.angular_rad_s == 0);
    if (!guard.busy() && (glide || (reverse && forward))) {
      assert(unsafe);
      // Independently prove the complete rear-axle quarter-turn footprint,
      // with 2 mm wall clearance before releasing it. The existing footprint
      // assertions below still check every subsequent simulation step.
      for (int sample = 0; sample <= 180; ++sample) {
        const float yaw = robot.yaw * (1.f - float(sample) / 180);
        const float x = robot.x + motor::cfg::wheel_mount_x_body_m *
                                      (std::cos(robot.yaw) - std::cos(yaw));
        const auto box = bounds(x, robot.y + motor::cfg::wheel_mount_x_body_m *
                                      (std::sin(robot.yaw) - std::sin(yaw)), yaw);
        assert(box.min_x > .402f && box.max_x < .598f);
      }
      assert(std::fabs(loc::wrap_pi(robot.yaw - loc::kPi / 2)) <=
             loc::deg2rad(2));
      const auto resumed =
          guard.update(map, pose(robot, now), f, motor, turn(), now);
      assert(resumed.angular_rad_s == turn().angular_rad_s);
      corrected = true;
      break;
    }
    const float half_track = motor::cfg::track_width_m / 2;
    motor::WheelCommand command{
        (request.linear_m_s - half_track * request.angular_rad_s) /
            motor::cfg::wheel_radius_m,
        (request.linear_m_s + half_track * request.angular_rad_s) /
            motor::cfg::wheel_radius_m,
        now};
    motor = wheels.update(command, now);
    robot.step(motor.duty_left, motor.duty_right, .01f);
    const auto box = bounds(robot.x, robot.y, robot.yaw);
    assert(!robot.collided && box.min_x > .4f && box.max_x < .6f);
    assert(box.min_y > 0 && box.max_y < 1);
  }
  assert(corrected && (glide || (reverse && forward)));
  // Execute the original quarter-turn after release, checking the full
  // footprint throughout the pivot and its final coast to rest.
  bool finished = false;
  const float axle_x =
      robot.x + motor::cfg::wheel_mount_x_body_m * std::cos(robot.yaw);
  const float axle_y =
      robot.y + motor::cfg::wheel_mount_x_body_m * std::sin(robot.yaw);
  int stopped = 0;
  for (int i = 0; i < 1200; ++i) {
    now += 10000;
    auto requested = turn();
    // A finite pivot rate must overcome static friction at the shorter track.
    requested.angular_rad_s = std::copysign(
        std::clamp(2 * std::fabs(robot.yaw), .08f, .5f), -robot.yaw);
    if (std::fabs(robot.yaw) <= loc::deg2rad(1))
      requested.angular_rad_s = 0;
    auto f = robot.sensors(now, 2000 + i);
    motor.timestamp_us = now;
    const auto d =
        guard.update(map, pose(robot, now), f, motor, requested, now);
    assert(!guard.busy() && std::fabs(d.linear_m_s) <= .008f);
    assert(
        std::hypot(
            robot.x + motor::cfg::wheel_mount_x_body_m * std::cos(robot.yaw) -
                axle_x,
            robot.y + motor::cfg::wheel_mount_x_body_m * std::sin(robot.yaw) -
                axle_y) < .001f);
    const float half_track = motor::cfg::track_width_m / 2;
    motor = wheels.update({(d.linear_m_s - half_track * d.angular_rad_s) /
                               motor::cfg::wheel_radius_m,
                           (d.linear_m_s + half_track * d.angular_rad_s) /
                               motor::cfg::wheel_radius_m,
                           now},
                          now);
    robot.step(motor.duty_left, motor.duty_right, .01f);
    const auto box = bounds(robot.x, robot.y, robot.yaw);
    assert(!robot.collided && box.min_x > .4f && box.max_x < .6f);
    stopped = d.angular_rad_s == 0 ? stopped + 1 : 0;
    if (stopped >= 50) {
      assert(std::hypot(robot.vx_m_s, robot.vy_m_s) < .001f &&
             std::fabs(robot.body_omega_rps) < .01f);
      finished = true;
      break;
    }
  }
  if (!finished)
    std::fprintf(stderr, "pivot incomplete yaw=%.5f omega=%.5f stopped=%d\n",
                 robot.yaw, robot.body_omega_rps, stopped);
  assert(finished);
  std::printf("PASS %s %s: coast, both drive directions, measured clearance, "
              "no scrape\n",
              glide ? "glide_stop" : "off_center", right ? "right" : "left");
}
// Reproduce the east-facing entrance: lateral-only recovery has no solution.
void entrance_longitudinal_recovery() {
  const auto map = loc::MazeMap::q1();
  sim::Noise noise{};
  noise.gyro_sigma = noise.gyro_bias = noise.mag_sigma = noise.range_sigma = 0;
  sim::Surface surface;
  surface.enabled = true;
  sim::Robot robot(map, {4, 0}, 0, 2743464134u, noise, {}, surface);
  nav::TurnGuard guard;
  motor::WheelController wheels;
  motor::MotorSnapshot m{};
  int64_t now = 1000000;
  bool translated = false, released = false;
  for (int i = 0; i < 1500; ++i) {
    now += 10000;
    auto request = turn();
    request.angular_rad_s = .5f;
    request.diagnostics.pivot_angle_rad = loc::kPi / 2;
    auto frame = robot.sensors(now, i + 1);
    m.timestamp_us = now;
    auto out = guard.update(map, pose(robot, now), frame, m, request, now);
    assert(guard.phase() != nav::TurnGuardPhase::BLOCKED);
    translated = translated || out.linear_m_s > 0;
    if (!guard.busy()) {
      assert(translated);
      // Independent square-corner envelope across the requested quarter turn.
      for (int j = 0; j <= 180; ++j) {
        const float a = robot.yaw + (loc::kPi / 2 - robot.yaw) * j / 180;
        const float x = robot.x + motor::cfg::wheel_mount_x_body_m *
                                      (std::cos(robot.yaw) - std::cos(a));
        const float y = robot.y + motor::cfg::wheel_mount_x_body_m *
                                      (std::sin(robot.yaw) - std::sin(a));
        const auto box = bounds(x, y, a);
        assert(box.min_x > .8f && box.max_x < 1.f && box.min_y > 0);
      }
      released = true;
      break;
    }
    const float b = motor::cfg::track_width_m / 2;
    m = wheels.update(
        {(out.linear_m_s - b * out.angular_rad_s) / motor::cfg::wheel_radius_m,
         (out.linear_m_s + b * out.angular_rad_s) / motor::cfg::wheel_radius_m,
         now},
        now);
    robot.step(m.duty_left, m.duty_right, .01f);
    const auto box = bounds(robot.x, robot.y, robot.yaw);
    assert(!robot.collided && box.min_x > .8f && box.max_x < 1.f && box.min_y > 0);
  }
  assert(released);
}
void narrow_start_interval(unsigned rotation) {
  const auto map =
      loc::MazeMap::q1().rotated(rotation, static_cast<loc::MapKind>(rotation));
  sim::Noise noise{};
  noise.gyro_sigma = noise.gyro_bias = noise.mag_sigma = noise.range_sigma = 0;
  sim::Surface surface;
  surface.enabled = true;
  sim::Robot robot(map, loc::rotate_cell({0, 0}, rotation),
                   rotation * loc::kPi / 2, 4159659801u, noise, {}, surface);
  nav::TurnGuard guard;
  motor::WheelController wheels;
  motor::MotorSnapshot m{};
  auto request = turn();
  request.angular_rad_s = .5f;
  request.diagnostics.pivot_angle_rad = loc::kPi / 2;
  int64_t now = 1000000;
  bool translated = false, released = false;
  for (int i = 0; i < 1500; ++i) {
    now += 10000;
    auto measured = pose(robot, now);
    // The measured offset/heading from the failing GDB sample. It places
    // the narrow safe interval between the old coarse grid's samples.
    float bx = .000165024f, by = .000501664f;
    for (unsigned j = 0; j < rotation; ++j) {
      const float previous = bx;
      bx = -by;
      by = previous;
    }
    measured.x_m += bx;
    measured.y_m += by;
    measured.yaw_rad += .000931731f;
    m.timestamp_us = now;
    const auto out =
        guard.update(map, measured, robot.sensors(now, i + 1), m, request, now);
    assert(guard.phase() != nav::TurnGuardPhase::BLOCKED);
    translated = translated || out.linear_m_s > 0;
    if (guard.pivot_authorized()) {
      assert(translated);
      // Independent physical corner bounds for the entire released arc.
      for (int j = 0; j <= 180; ++j) {
        const float yaw = robot.yaw + loc::kPi / 2 * j / 180;
        float x = robot.x + motor::cfg::wheel_mount_x_body_m *
                                (std::cos(robot.yaw) - std::cos(yaw));
        float y = robot.y + motor::cfg::wheel_mount_x_body_m *
                                (std::sin(robot.yaw) - std::sin(yaw));
        for (float sx : {-.0565f, .055f})
          for (float sy : {-.0675f, .0675f}) {
            float cx = x + sx * std::cos(yaw) - sy * std::sin(yaw);
            float cy = y + sx * std::sin(yaw) + sy * std::cos(yaw);
            for (unsigned k = 0; k < rotation; ++k) {
              const float previous = cx;
              cx = cy;
              cy = 1 - previous;
            }
            assert(cx > 0 && cx < .2f && cy > 0 && cy < .4f);
          }
      }
      released = true;
      break;
    }
    const float b = motor::cfg::track_width_m / 2;
    m = wheels.update(
        {(out.linear_m_s - b * out.angular_rad_s) / motor::cfg::wheel_radius_m,
         (out.linear_m_s + b * out.angular_rad_s) / motor::cfg::wheel_radius_m,
         now},
        now);
    robot.step(m.duty_left, m.duty_right, .01f);
    assert(!robot.collided);
  }
  assert(released);
}

void authorized_pivot_requires_fresh_heading_and_ranges() {
  const auto map = corridor();
  sim::Noise noise{};
  noise.gyro_sigma = noise.gyro_bias = noise.mag_sigma = noise.range_sigma = 0;
  sim::Robot robot(map, {2, 2}, loc::kPi / 2, 71, noise);
  robot.x = .487f;
  nav::TurnGuard guard;
  motor::MotorSnapshot m{};
  int64_t now = 1000000;
  for (int i = 0; i < 60; ++i) {
    now += 10000;
    m.timestamp_us = now;
    guard.update(map, pose(robot, now), robot.sensors(now, i + 1), m, turn(),
                 now);
  }
  assert(guard.pivot_authorized());
  auto p = pose(robot, now);
  p.tracking = false;
  p.timestamp_us -= 1000000;
  auto frame = robot.sensors(now, 100);
  nav::HeadingObservation heading{robot.yaw, now, true};
  auto out = guard.update(map, p, frame, m, turn(), now, &heading);
  assert(out.angular_rad_s == turn().angular_rad_s && out.linear_m_s == 0);
  // The guard's local envelope must not turn a stale pose into localization.
  assert(!p.tracking && p.timestamp_us == now - 1000000);
  heading.timestamp_us -= 100001;
  out = guard.update(map, p, frame, m, turn(), now, &heading);
  assert(out.angular_rad_s == 0 && out.linear_m_s == 0);
  heading.timestamp_us = now;
  frame.tof_us[0] -= 150001;
  out = guard.update(map, p, frame, m, turn(), now, &heading);
  assert(out.angular_rad_s == 0 && out.linear_m_s == 0);
  guard.cancel();
  assert(!guard.pivot_authorized());
  frame = robot.sensors(now, 101);
  out = guard.update(map, p, frame, m, turn(), now, &heading);
  assert(out.angular_rad_s == 0 && out.linear_m_s == 0);
}
void half_turn_keeps_requested_direction_across_pi() {
  const auto map = corridor().rotated(1, loc::MapKind::Q2);
  for (float sign : {-1.f, 1.f}) {
    sim::Noise noise{};
    noise.gyro_sigma = noise.gyro_bias = noise.mag_sigma = noise.range_sigma =
        0;
    sim::Robot robot(map, {2, 2}, sign * .001f, 71, noise);
    // Keep the axle centered so the complete v2 half-turn remains both
    // collision-free and observable with the forward-shifted side sensors.
    robot.x = .5f + .024f * std::cos(robot.yaw);
    robot.y = .5f - sign * .014f + .024f * std::sin(robot.yaw);
    nav::TurnGuard guard;
    motor::MotorSnapshot m{};
    auto request = turn();
    request.angular_rad_s = sign * .5f;
    request.diagnostics.pivot_angle_rad = sign * loc::kPi;
    int64_t now = 1000000;
    for (int i = 0; i < 60; ++i) {
      now += 10000;
      m.timestamp_us = now;
      guard.update(map, pose(robot, now), robot.sensors(now, i + 1), m, request,
                   now);
    }
    assert(guard.pivot_authorized());
    robot.yaw = 0; // The shortest-angle wrap now points the other way.
    now += 10000;
    m.timestamp_us = now;
    request.angular_rad_s = -request.angular_rad_s;
    const auto out = guard.update(map, pose(robot, now), robot.sensors(now, 61),
                                  m, request, now);
    assert(guard.pivot_authorized() &&
           out.angular_rad_s == -request.angular_rad_s);
  }
}
// Both coordinates need correction: a pure sideways move still leaves the
// rear-axle arc too close to the west wall; a straight move leaves it too
// close to the south wall. The complete maneuver must use both drive signs.
void fore_aft_shunt_makes_room_for_turn(unsigned rotation) {
  auto map = corridor();
  for (int row = 0; row < 5; ++row) {
    map.walls[row][2] &= ~loc::EAST;
    map.walls[row][3] &= ~loc::WEST;
  }
  map = map.rotated(rotation, static_cast<loc::MapKind>(rotation));
  sim::Noise noise{};
  noise.gyro_sigma = noise.gyro_bias = noise.mag_sigma = noise.range_sigma = 0;
  sim::Surface surface;
  surface.enabled = true;
  sim::Robot robot(map, loc::rotate_cell({2, 0}, rotation),
                   rotation * loc::kPi / 2, 2743464134u, noise, {}, surface);
  float x = .492f, y = .090f;
  for (unsigned r = 0; r < rotation; ++r) {
    const float previous_x = x;
    x = 1 - y;
    y = previous_x;
  }
  robot.x = x;
  robot.y = y;
  nav::TurnGuard guard;
  motor::WheelController wheels;
  motor::MotorSnapshot m{};
  auto request = turn();
  request.angular_rad_s = .5f;
  request.diagnostics.pivot_angle_rad = loc::kPi / 2;
  int64_t now = 1000000;
  bool forward = false, reverse = false, released = false;
  for (int i = 0; i < 1500; ++i) {
    now += 10000;
    const auto frame = robot.sensors(now, i + 1);
    m.timestamp_us = now;
    const auto out =
        guard.update(map, pose(robot, now), frame, m, request, now);
    if (guard.phase() == nav::TurnGuardPhase::BLOCKED)
      std::fprintf(stderr,
                   "shunt blocked rotation=%u t=%.2f pose=%.5f,%.5f,%.4f\n",
                   rotation, i * .01, robot.x, robot.y, robot.yaw);
    assert(guard.phase() != nav::TurnGuardPhase::BLOCKED);
    forward = forward || out.linear_m_s > 0;
    reverse = reverse || out.linear_m_s < 0;
    // Check every corner against the actual walls, independently of the
    // guard's mapped rectangle/segment implementation.
    auto check = [&](float cx, float cy, float yaw, float padding) {
      for (float sx : {-.0565f - padding, .055f + padding})
        for (float sy : {-.0675f - padding, .0675f + padding}) {
          float px = cx + sx * std::cos(yaw) - sy * std::sin(yaw);
          float py = cy + sx * std::sin(yaw) + sy * std::cos(yaw);
          for (unsigned r = 0; r < rotation; ++r) {
            const float previous_x = px;
            px = py;
            py = 1 - previous_x;
          }
          assert(px > .4f && px < .8f && py > 0 && py < 1);
        }
    };
    check(robot.x, robot.y, robot.yaw, 0);
    assert(!robot.collided);
    if (guard.pivot_authorized()) {
      assert(forward); // V2 can correct this posture with a straight fore/aft shunt.
      const float target = rotation * loc::kPi / 2 + loc::kPi / 2;
      const float angle = loc::wrap_pi(target - robot.yaw);
      for (int sample = 0; sample <= 180; ++sample) {
        const float yaw = robot.yaw + angle * sample / 180;
        const float axle = motor::cfg::wheel_mount_x_body_m;
        check(robot.x + axle * (std::cos(robot.yaw) - std::cos(yaw)),
              robot.y + axle * (std::sin(robot.yaw) - std::sin(yaw)), yaw,
              .002f);
      }
      const float longitudinal =
          (robot.x - x) * std::cos(rotation * loc::kPi / 2) +
          (robot.y - y) * std::sin(rotation * loc::kPi / 2);
      assert(std::fabs(longitudinal) > .010f);
      // Execute the released turn and its coast, instead of validating only
      // the planned endpoint. This exercises the actual wheel/axle dynamics.
      int stopped = 0;
      for (int tick = 0; tick < 1200; ++tick) {
        now += 10000;
        auto pivot = request;
        const float error = loc::wrap_pi(target - robot.yaw);
        pivot.angular_rad_s =
            std::fabs(error) <= loc::deg2rad(1)
                ? 0
                : std::copysign(std::clamp(2 * std::fabs(error), .08f, .5f),
                                error);
        pivot.diagnostics.pivot_angle_rad = error;
        m.timestamp_us = now;
        const auto command =
            guard.update(map, pose(robot, now), robot.sensors(now, 2000 + tick),
                         m, pivot, now);
        assert(guard.pivot_authorized());
        const float b = motor::cfg::track_width_m / 2;
        m = wheels.update({(command.linear_m_s - b * command.angular_rad_s) /
                               motor::cfg::wheel_radius_m,
                           (command.linear_m_s + b * command.angular_rad_s) /
                               motor::cfg::wheel_radius_m,
                           now},
                          now);
        robot.step(m.duty_left, m.duty_right, .01f);
        check(robot.x, robot.y, robot.yaw, 0);
        assert(!robot.collided);
        stopped = command.angular_rad_s == 0 ? stopped + 1 : 0;
        if (stopped >= 50) {
          assert(std::hypot(robot.vx_m_s, robot.vy_m_s) < .001f &&
                 std::fabs(robot.body_omega_rps) < .01f);
          released = true;
          break;
        }
      }
      std::printf(
          "fore/aft shunt rotation=%u forward=%d reverse=%d finished=%d\n",
          rotation, forward, reverse, released);
      break;
    }
    const float b = motor::cfg::track_width_m / 2;
    m = wheels.update(
        {(out.linear_m_s - b * out.angular_rad_s) / motor::cfg::wheel_radius_m,
         (out.linear_m_s + b * out.angular_rad_s) / motor::cfg::wheel_radius_m,
         now},
        now);
    robot.step(m.duty_left, m.duty_right, .01f);
  }
  assert(released);
}
void safety() {
  static_assert(sizeof(nav::TurnGuard) <= 128,
                "Bounded task-owned guard state");
  const auto map = corridor();
  sim::Robot robot(map, {2, 2}, loc::kPi / 2, 1);
  robot.x = .492f;
  nav::TurnGuard guard;
  int64_t now = 1000000;
  motor::MotorSnapshot m{};
  m.timestamp_us = now;
  auto f = robot.sensors(now, 1);
  auto p = pose(robot, now);
  f.range_m[0] = std::numeric_limits<float>::quiet_NaN();
  auto d = guard.update(map, p, f, m, turn(), now);
  assert(d.linear_m_s == 0 && d.angular_rad_s == 0 && guard.busy());
  f = robot.sensors(now, 2);
  p.tracking = false;
  d = guard.update(map, p, f, m, turn(), now);
  assert(d.linear_m_s == 0 && d.angular_rad_s == 0);
  p = pose(robot, now);
  f.tof_us[2] -= 200000;
  d = guard.update(map, p, f, m, turn(), now);
  assert(d.linear_m_s == 0 && d.angular_rad_s == 0);
  now += 15000001;
  d = guard.update(map, p, f, m, turn(), now);
  assert(d.status == nav::ExecuteStatus::BLOCKED && d.linear_m_s == 0 &&
         d.angular_rad_s == 0);
  guard.cancel();
  assert(!guard.busy());
  // No invented rear clearance: unknown surrounding edges prevent recovery.
  robot.x = .508f; // V2 needs recovery across the unknown cell edge.
  auto unknown = map;
  for (auto &row : unknown.known)
    for (auto &cell : row)
      cell = 0;
  for (int i = 0; i < 60; ++i) {
    now += 10000;
    p = pose(robot, now);
    f = robot.sensors(now, i + 3);
    m.timestamp_us = now;
    d = guard.update(unknown, p, f, m, turn(), now);
    assert(d.linear_m_s == 0 && d.angular_rad_s == 0);
  }
  assert(guard.phase() == nav::TurnGuardPhase::BLOCKED);
  std::puts(
      "PASS invalid/stale inputs, bounded timeout, cancel, unknown rear map");
}
void invalid_ranges_do_not_erase_quiet_motor_dwell() {
  const auto map = corridor();
  sim::Noise noise{};
  noise.gyro_sigma = noise.gyro_bias = noise.mag_sigma = noise.range_sigma = 0;
  sim::Robot robot(map, {2, 2}, loc::kPi / 2, 1, noise);
  robot.x = .487f;
  for (int interruption = 0; interruption < 3; ++interruption) {
    nav::TurnGuard guard;
    motor::MotorSnapshot m{};
    int64_t now = 1000000;
    for (int i = 0; i < 60; ++i) {
      now += 10000;
      auto f = robot.sensors(now, i + 1);
      f.range_m[2] = .014f;
      m.timestamp_us = now;
      const auto d = guard.update(map, pose(robot, now), f, m, turn(), now);
      assert(d.linear_m_s == 0 && d.angular_rad_s == 0);
    }
    if (interruption == 1)
      now += 60000;
    for (int i = 0; i < 5; ++i) {
      now += 10000;
      auto f = robot.sensors(now, 61 + i);
      m.timestamp_us = now;
      m.duty_left = interruption == 2 && i == 0 ? .1f : 0;
      const auto d = guard.update(map, pose(robot, now), f, m, turn(), now);
      if (interruption || i < 3)
        assert(d.linear_m_s == 0 && d.angular_rad_s == 0);
    }
    assert(guard.busy() == (interruption != 0));
  }
}

void centered_noise_waits_without_releasing_a_pivot() {
  const auto map = corridor();
  sim::Robot robot(map, {2, 2}, loc::kPi / 2, 1);
  // A rear-axle clockwise quarter-turn starts at this safe biased posture,
  // rather than at the platform center used by the former centered axle.
  robot.x = .487f;
  nav::TurnGuard guard;
  int64_t now = 1000000;
  motor::MotorSnapshot m{};
  nav::DriveRequest d{};
  for (int i = 0; i < 60; ++i) {
    now += 10000;
    auto f = robot.sensors(now, i + 1);
    f.imu.gz_rps = 0;
    f.range_m[0] = robot.x - .4f - loc::cfg::platform_half_side_m;
    f.range_m[2] = .6f - robot.x - .014f - loc::cfg::platform_half_side_m;
    m.timestamp_us = now;
    d = guard.update(map, pose(robot, now), f, m, turn(), now);
    assert(d.linear_m_s == 0 && d.angular_rad_s == 0);
    assert(guard.phase() == nav::TurnGuardPhase::COAST);
  }
  now += 10000;
  auto f = robot.sensors(now, 61);
  f.imu.gz_rps = 0;
  f.range_m[0] = robot.x - .4f - loc::cfg::platform_half_side_m;
  f.range_m[2] = .6f - robot.x - loc::cfg::platform_half_side_m;
  m.timestamp_us = now;
  d = guard.update(map, pose(robot, now), f, m, turn(), now);
  assert(!guard.busy() && d.angular_rad_s == turn().angular_rad_s);
  // Once released, ordinary pivot timing belongs to the caller. Keep the
  // separate active-correction timeout tests above unchanged.
  now += 16000000;
  f = robot.sensors(now, 62);
  f.imu.gz_rps = 0;
  f.range_m[0] = robot.x - .4f - loc::cfg::platform_half_side_m;
  f.range_m[2] = .6f - robot.x - loc::cfg::platform_half_side_m;
  m.timestamp_us = now;
  d = guard.update(map, pose(robot, now), f, m, turn(), now);
  assert(!guard.busy() && d.angular_rad_s == turn().angular_rad_s);
  nav::DriveRequest waiting{};
  waiting.status = nav::ExecuteStatus::FALLBACK;
  guard.update(map, pose(robot, now), f, m, waiting, now);
  d = guard.update(map, pose(robot, now), f, m, turn(), now);
  assert(!guard.busy() && d.angular_rad_s == turn().angular_rad_s);
  // An actual translation invalidates that permission.
  waiting.linear_m_s = .01f;
  guard.update(map, pose(robot, now), f, m, waiting, now);
  d = guard.update(map, pose(robot, now), f, m, turn(), now);
  assert(guard.busy() && d.angular_rad_s == 0);
}
} // namespace
int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string mode = argv[1];
  if (mode == "safety") {
    for (unsigned rotation = 0; rotation < 4; ++rotation)
      fore_aft_shunt_makes_room_for_turn(rotation);
    half_turn_keeps_requested_direction_across_pi();
    authorized_pivot_requires_fresh_heading_and_ranges();
    entrance_longitudinal_recovery();
    for (unsigned rotation = 0; rotation < 4; ++rotation)
      narrow_start_interval(rotation);
    safety();
    invalid_ranges_do_not_erase_quiet_motor_dwell();
    centered_noise_waits_without_releasing_a_pivot();
  } else {
    assert(mode == "off_center" || mode == "glide_stop");
    recovery(mode == "glide_stop", false);
    recovery(mode == "glide_stop", true);
  }
}
