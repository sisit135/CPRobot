#pragma once
#include "motor_control/types.hpp"
#include "navigation/types.hpp"
#include <random>
namespace sim {
struct Noise {
  float gyro_sigma = .002f, gyro_bias = .003f, range_sigma = .002f,
        mag_sigma = .001f;
};
struct Plant {
  float left_gain = 20, right_gain = 19, left_tau_s = .15f, right_tau_s = .17f;
};
/** @brief Independent differential-drive plant and simulated sensor library.
 * @note Uses std::mt19937 with a caller seed; no runtime dependency downloads.
 * Ground-truth range uses grid DDA, independently of production segment
 * tracing.
 */
class Robot {
public:
  /** @brief Construct a stationary plant in a map cell.
   * @param[in] map Physical maze for simulation.
   * @param[in] cell Initial cell.
   * @param[in] yaw Initial world heading in radians.
   * @param[in] seed Random seed, fixed by each test.
   * @param[in] noise Sensor noise parameters in SI units.
   * @param[in] plant Independent PWM-to-speed plant parameters, hidden from
   * control.
   * @returns No value (constructor).
   */
  Robot(loc::MazeMap map, loc::Cell cell, float yaw, uint32_t seed,
        Noise noise = {}, Plant plant = {});
  /** @brief Integrate a first-order DC wheel plant from signed PWM requests.
   * @param[in] left Left duty [-1,+1].
   * @param[in] right Right duty [-1,+1].
   * @param[in] dt Step in seconds.
   * @returns No value; collision is recorded in collided.
   */
  void step(float left, float right, float dt);
  /** @brief Generate fresh IMU, MAG, TCS and three ToF frames.
   * @param[in] now Shared acquisition time in microseconds.
   * @param[in] sequence Fresh optical sequence number.
   * @returns Snapshot with seeded Gaussian noise and physical ray distances.
   */
  nav::BootstrapFrame sensors(int64_t now, uint32_t sequence);
  // Ground truth only; the controller and executor never receive wl/wr.
  float x, y, yaw, wl = 0, wr = 0;
  bool collided = false;

private:
  float range(const loc::LidarMount &mount) const;
  float gaussian(float sigma);
  loc::MazeMap map_;
  Noise noise_;
  Plant plant_;
  std::mt19937 rng_;
  std::normal_distribution<float> normal_{0, 1};
  float omega_ = 0;
};
} // namespace sim
