#include "localization/config.hpp"
#include "localization/robot_ekf.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
  loc::RobotEKF f;
  f.reset(.495f, .505f, .02f);
  for (int k = 0; k < 1000; ++k) {
    loc::ImuSample imu{};
    imu.gz_rps = .01f * std::sin(k * .01f) + .003f;
    assert(f.predict(imu, .01f));
    if (k % 5 == 0) {
      const float yaw = .01f * std::sin(k * .01f);
      f.update_mag({45 * std::cos(yaw), -45 * std::sin(yaw), 0});
      for (int i = 0; i < 3; ++i)
        f.update_lidar(loc::RobotEKF::expected_range(.5f, .5f, yaw,
                                                     loc::cfg::lidar_mounts[i]),
                       loc::cfg::lidar_mounts[i]);
    }
    for (int i = 0; i < 4; ++i)
      std::printf("%.9g ", f.state(i));
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        std::printf("%.9g ", f.covariance(i, j));
    std::puts("");
  }
}
