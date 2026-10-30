#include "robot_sim.hpp"
#include "localization/config.hpp"
#include "motor_control/config.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace sim {
Robot::Robot(loc::MazeMap m, loc::Cell c, float a, uint32_t s, Noise n, Plant p)
    : x((c.x + .5f) * .2f), y((c.y + .5f) * .2f), yaw(a), map_(m), noise_(n),
      plant_(p), rng_(s) {}
float Robot::gaussian(float sigma) { return sigma * normal_(rng_); }
float Robot::range(const loc::LidarMount &m) const {
  const float c = std::cos(yaw), s = std::sin(yaw),
              ox = x + c * m.x_body_m - s * m.y_body_m,
              oy = y + s * m.x_body_m + c * m.y_body_m;
  const float dx = std::cos(yaw + m.yaw_body_rad),
              dy = std::sin(yaw + m.yaw_body_rad);
  int cx = int(std::floor(ox / .2f)), cy = int(std::floor(oy / .2f));
  if (cx < 0 || cx >= 5 || cy < 0 || cy >= 5)
    return NAN;
  // Direct access to raw masks, independent edge implementation.
  auto closed = [&](int a, int b, uint8_t bit, int na, int nb,
                    uint8_t opposite) {
    bool wall = map_.walls[4 - b][a] & bit;
    if (na >= 0 && na < 5 && nb >= 0 && nb < 5)
      wall = wall || (map_.walls[4 - nb][na] & opposite);
    return wall;
  };
  for (int step = 0; step < 12; ++step) {
    const float tx = std::fabs(dx) < 1e-6f
                         ? INFINITY
                         : ((dx > 0 ? (cx + 1) * .2f : cx * .2f) - ox) / dx;
    const float ty = std::fabs(dy) < 1e-6f
                         ? INFINITY
                         : ((dy > 0 ? (cy + 1) * .2f : cy * .2f) - oy) / dy;
    if (tx < ty) {
      int nx = cx + (dx > 0 ? 1 : -1);
      if (closed(cx, cy, dx > 0 ? loc::EAST : loc::WEST, nx, cy,
                 dx > 0 ? loc::WEST : loc::EAST))
        return tx;
      cx = nx;
    } else {
      int ny = cy + (dy > 0 ? 1 : -1);
      if (closed(cx, cy, dy > 0 ? loc::NORTH : loc::SOUTH, cx, ny,
                 dy > 0 ? loc::SOUTH : loc::NORTH))
        return ty;
      cy = ny;
    }
    if (cx < 0 || cx >= 5 || cy < 0 || cy >= 5)
      return NAN;
  }
  return NAN;
}
void Robot::step(float l, float r, float dt) {
  wl += (plant_.left_gain * std::clamp(l, -1.f, 1.f) - wl) * dt /
        plant_.left_tau_s;
  wr += (plant_.right_gain * std::clamp(r, -1.f, 1.f) - wr) * dt /
        plant_.right_tau_s;
  const float v = (wl + wr) * motor::cfg::wheel_radius_m / 2;
  omega_ = (wr - wl) * motor::cfg::wheel_radius_m / motor::cfg::track_width_m;
  const float nx = x + v * std::cos(yaw + omega_ * dt / 2) * dt,
              ny = y + v * std::sin(yaw + omega_ * dt / 2) * dt;
  const loc::Cell before{int(std::floor(x / .2f)), int(std::floor(y / .2f))},
      after{int(std::floor(nx / .2f)), int(std::floor(ny / .2f))};
  if (!loc::valid_cell(after) ||
      (before != after && !map_.can_step(before, after)))
    collided = true;
  x = nx;
  y = ny;
  yaw = loc::wrap_pi(yaw + omega_ * dt);
}
nav::BootstrapFrame Robot::sensors(int64_t now, uint32_t seq) {
  nav::BootstrapFrame f{};
  f.color = {800, 100, 100, 1200, seq, false};
  f.color_us = f.imu_us = f.mag_us = now;
  f.imu.gz_rps = omega_ + noise_.gyro_bias + gaussian(noise_.gyro_sigma);
  f.imu.ax_mps2 = gaussian(.01f);
  f.imu.ay_mps2 = gaussian(.01f);
  f.imu.az_mps2 = 9.80665f + gaussian(.01f);
  const float a = yaw + gaussian(noise_.mag_sigma);
  f.mag = {45 * std::cos(a), -45 * std::sin(a), 0};
  f.magnetic_clean = std::fabs(wl) < .05f && std::fabs(wr) < .05f;
  for (int i = 0; i < 3; ++i) {
    f.tof_us[i] = now;
    f.range_m[i] =
        range(loc::cfg::lidar_mounts[i]) + gaussian(noise_.range_sigma);
  }
  return f;
}
} // namespace sim
