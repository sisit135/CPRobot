#include "localization/robot_ekf.hpp"
#include "localization/config.hpp"
#include "localization/matrix_backend.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace loc {
float wrap_pi(float angle) { return std::remainder(angle, 2.0f * kPi); }

RobotEKF::RobotEKF() {
  reset(cfg::initial_x_m, cfg::initial_y_m, cfg::initial_yaw_rad);
}
void RobotEKF::reset(float x, float y, float yaw, float position_sigma,
                     float yaw_sigma) {
  std::memset(x_, 0, sizeof(x_));
  std::memset(P_, 0, sizeof(P_));
  x_[X] = std::isfinite(x) ? x : cfg::initial_x_m;
  x_[Y] = std::isfinite(y) ? y : cfg::initial_y_m;
  x_[YAW] = std::isfinite(yaw) ? wrap_pi(yaw) : cfg::initial_yaw_rad;
  if (!std::isfinite(position_sigma) || position_sigma <= 0)
    position_sigma = .15f;
  if (!std::isfinite(yaw_sigma) || yaw_sigma <= 0)
    yaw_sigma = deg2rad(25);
  P_[X][X] = P_[Y][Y] = position_sigma * position_sigma;
  P_[YAW][YAW] = yaw_sigma * yaw_sigma;
  P_[BGZ][BGZ] = deg2rad(5) * deg2rad(5);
}

bool RobotEKF::predict(const ImuSample &imu, float dt) {
  if (!std::isfinite(dt) || dt <= 0 || dt > 0.05f ||
      !std::isfinite(imu.gz_rps) || std::fabs(imu.gz_rps) > 30)
    return false;
  // Event model: hold position mean; do not integrate acceleration or PWM.
  float F[N][N]{};
  for (int i = 0; i < N; ++i)
    F[i][i] = 1;
  F[YAW][BGZ] = -dt;
  float FP[N][N]{}, Ft[N][N]{}, next[N][N]{};
  if (!matrix::multiply(&F[0][0], &P_[0][0], &FP[0][0]) ||
      !matrix::transpose(&F[0][0], &Ft[0][0]) ||
      !matrix::multiply(&FP[0][0], &Ft[0][0], &next[0][0]))
    return false;
  x_[YAW] = wrap_pi(x_[YAW] + (imu.gz_rps - x_[BGZ]) * dt);
  const float qp = cfg::sigma_position_rw * cfg::sigma_position_rw;
  const float qg = cfg::sigma_gyro_rw * cfg::sigma_gyro_rw;
  const float qb = cfg::sigma_bias_rw * cfg::sigma_bias_rw;
  next[X][X] += qp * dt;
  next[Y][Y] += qp * dt;
  next[YAW][YAW] += qg * dt + qb * dt * dt * dt / 3;
  next[YAW][BGZ] -= qb * dt * dt / 2;
  next[BGZ][YAW] -= qb * dt * dt / 2;
  next[BGZ][BGZ] += qb * dt;
  std::memcpy(P_, next, sizeof(P_));
  return true;
}

void RobotEKF::timing_gap(float dt) {
  const float duration = std::isfinite(dt) ? std::max(dt, 0.0f) : 10.0f;
  const float qp = cfg::sigma_position_rw * cfg::sigma_position_rw;
  P_[X][X] += qp * duration;
  P_[Y][Y] += qp * duration;
  P_[YAW][YAW] += deg2rad(30) * deg2rad(30) * duration;
  P_[BGZ][BGZ] += cfg::sigma_bias_rw * cfg::sigma_bias_rw * duration;
}

bool RobotEKF::update_mag(const MagSample &m) {
  if (!std::isfinite(m.mx_uT) || !std::isfinite(m.my_uT) ||
      !std::isfinite(m.mz_uT))
    return false;
  const float norm =
      std::sqrt(m.mx_uT * m.mx_uT + m.my_uT * m.my_uT + m.mz_uT * m.mz_uT);
  if (norm < cfg::mag_min_uT || norm > cfg::mag_max_uT ||
      m.mx_uT * m.mx_uT + m.my_uT * m.my_uT < 1)
    return false;
  // For +Y left and CCW yaw, body field = [B*cos(yaw), -B*sin(yaw)].
  const float heading =
      wrap_pi(std::atan2(-m.my_uT, m.mx_uT) + cfg::mag_heading_offset_rad);
  float H[N]{};
  H[YAW] = 1;
  return update_scalar(heading, x_[YAW], H,
                       cfg::sigma_mag_rad * cfg::sigma_mag_rad, true, 9);
}

bool RobotEKF::set_maze(const MazeMap &map) {
  if (!map.valid())
    return false;
  maze_ = map;
  return true;
}
float RobotEKF::expected_range(float x, float y, float yaw,
                               const LidarMount &m) {
  return expected_range(MazeMap::q1(), x, y, yaw, m);
}
float RobotEKF::expected_range(const MazeMap &map, float x, float y, float yaw,
                               const LidarMount &m) {
  return trace_maze_ray(map, x, y, yaw, m, cfg::corner_range_gap_m).range;
}

bool RobotEKF::update_lidar(float range, const LidarMount &m, int *axis) {
  if (!std::isfinite(range) || range <= cfg::tof_min_m ||
      range >= cfg::tof_max_m)
    return false;
  const MazeRayHit hit =
      trace_maze_ray(maze_, x_[X], x_[Y], x_[YAW], m, cfg::corner_range_gap_m);
  if (hit.status != RayStatus::HIT || hit.ambiguous ||
      !std::isfinite(hit.range))
    return false; // Wall association discontinuity near corners.
  const float c = std::cos(x_[YAW]), s = std::sin(x_[YAW]);
  const float dox = -s * m.x_body_m - c * m.y_body_m;
  const float doy = c * m.x_body_m - s * m.y_body_m;
  const float a = x_[YAW] + m.yaw_body_rad;
  const float dx = std::cos(a), dy = std::sin(a);
  float H[N]{};
  if (hit.axis == 0) {
    H[X] = -1 / dx;
    H[YAW] = (-dox + hit.range * dy) / dx;
  } else {
    H[Y] = -1 / dy;
    H[YAW] = (-doy - hit.range * dx) / dy;
  }
  const bool ok = update_scalar(range, hit.range, H,
                                cfg::sigma_tof_m * cfg::sigma_tof_m, false, 9);
  if (ok && axis)
    *axis = hit.axis == 0 ? X : Y;
  return ok;
}

bool RobotEKF::update_marker(const MarkerSample &m) {
  if (!m.detected || !std::isfinite(m.confidence) ||
      m.confidence < cfg::min_marker_confidence || m.confidence > 1)
    return false;
  if (m.geometry == MarkerGeometry::X_LINE ||
      m.geometry == MarkerGeometry::Y_LINE) {
    const int axis = m.geometry == MarkerGeometry::X_LINE ? X : Y;
    const int index = axis == X ? m.gx : m.gy;
    if (index < 0 || index > cfg::grid_n)
      return false; // Six lines, five cells.
    const float c = std::cos(x_[YAW]), s = std::sin(x_[YAW]);
    const float ox = c * cfg::color_x_body_m - s * cfg::color_y_body_m;
    const float oy = s * cfg::color_x_body_m + c * cfg::color_y_body_m;
    float H[N]{};
    H[axis] = 1;
    H[YAW] = axis == X ? -oy : ox;
    return update_scalar(
        index * cfg::cell_m, x_[axis] + (axis == X ? ox : oy), H,
        cfg::sigma_line_m * cfg::sigma_line_m / m.confidence, false, 16);
  }
  if (m.geometry != MarkerGeometry::CELL || m.gx < 0 || m.gx >= cfg::grid_n ||
      m.gy < 0 || m.gy >= cfg::grid_n)
    return false;
  // A full-cell marker means membership in an interval, not an exact center.
  // If already consistent, do NOT repeatedly collapse covariance to the center.
  const RobotEKF before = *this;
  const float variance = cfg::cell_m * cfg::cell_m / (12 * m.confidence);
  for (int axis = 0; axis < 2; ++axis) {
    const float c = std::cos(x_[YAW]), s = std::sin(x_[YAW]);
    const float offset =
        axis == 0 ? c * cfg::color_x_body_m - s * cfg::color_y_body_m
                  : s * cfg::color_x_body_m + c * cfg::color_y_body_m;
    const float derivative =
        axis == 0 ? -s * cfg::color_x_body_m - c * cfg::color_y_body_m
                  : c * cfg::color_x_body_m - s * cfg::color_y_body_m;
    const int idx = axis == 0 ? X : Y, cell = axis == 0 ? m.gx : m.gy;
    const float lower = cell * cfg::cell_m, upper = lower + cfg::cell_m;
    const float predicted = x_[idx] + offset;
    if (predicted >= lower && predicted < upper)
      continue;
    float H[N]{};
    H[idx] = 1;
    H[YAW] = derivative;
    if (!update_scalar(0.5f * (lower + upper), predicted, H, variance, false,
                       16)) {
      *this = before; // Atomic X/Y correction on gating failure.
      return false;
    }
  }
  return true;
}

bool RobotEKF::update_scalar(float z, float h, const float H[N], float R,
                             bool angle, float gate) {
  if (!std::isfinite(z) || !std::isfinite(h) || !std::isfinite(R) || R <= 0)
    return false;
  float PH[N]{};
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j)
      PH[i] += P_[i][j] * H[j];
  float S = R;
  for (int i = 0; i < N; ++i)
    S += H[i] * PH[i];
  if (!std::isfinite(S) || S <= 1e-12f)
    return false;
  const float innovation = angle ? wrap_pi(z - h) : z - h;
  if (!std::isfinite(innovation) || innovation * innovation / S > gate)
    return false;
  float K[N]{};
  for (int i = 0; i < N; ++i)
    K[i] = PH[i] / S;
  float A[N][N]{}, AP[N][N]{}, next[N][N]{};
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j)
      A[i][j] = (i == j ? 1.0f : 0.0f) - K[i] * H[j];
  float At[N][N]{};
  if (!matrix::multiply(&A[0][0], &P_[0][0], &AP[0][0]) ||
      !matrix::transpose(&A[0][0], &At[0][0]) ||
      !matrix::multiply(&AP[0][0], &At[0][0], &next[0][0]))
    return false;
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j)
      next[i][j] += K[i] * R * K[j];
  for (int i = 0; i < N; ++i) {
    if (!std::isfinite(x_[i] + K[i] * innovation))
      return false;
    for (int j = 0; j < N; ++j)
      if (!std::isfinite(next[i][j]))
        return false;
  }
  for (int i = 0; i < N; ++i)
    x_[i] += K[i] * innovation;
  x_[YAW] = wrap_pi(x_[YAW]);
  // Joseph form, symmetry enforced. No hard position clamping.
  for (int i = 0; i < N; ++i)
    for (int j = i; j < N; ++j)
      P_[i][j] = P_[j][i] = 0.5f * (next[i][j] + next[j][i]);
  return true;
}

PoseSnapshot RobotEKF::pose() const {
  PoseSnapshot p{};
  p.x_m = x_[X];
  p.y_m = x_[Y];
  p.yaw_rad = x_[YAW];
  p.gyro_bias_rps = x_[BGZ];
  p.sigma_x_m = std::sqrt(std::max(0.0f, P_[X][X]));
  p.sigma_y_m = std::sqrt(std::max(0.0f, P_[Y][Y]));
  p.sigma_yaw_rad = std::sqrt(std::max(0.0f, P_[YAW][YAW]));
  if (p.x_m >= 0 && p.x_m < cfg::map_w_m && p.y_m >= 0 &&
      p.y_m < cfg::map_h_m) {
    p.grid_x = static_cast<int>(std::floor(p.x_m / cfg::cell_m));
    p.grid_y = static_cast<int>(std::floor(p.y_m / cfg::cell_m));
    const float x0 = p.grid_x * cfg::cell_m, y0 = p.grid_y * cfg::cell_m;
    p.grid_confident = p.x_m - 2 * p.sigma_x_m >= x0 &&
                       p.x_m + 2 * p.sigma_x_m < x0 + cfg::cell_m &&
                       p.y_m - 2 * p.sigma_y_m >= y0 &&
                       p.y_m + 2 * p.sigma_y_m < y0 + cfg::cell_m;
  }
  return p;
}
} // namespace loc
