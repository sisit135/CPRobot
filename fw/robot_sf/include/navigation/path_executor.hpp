#pragma once
#include "AStar.hpp"
#include "motor_control/types.hpp"
#include "navigation/types.hpp"
namespace nav {
enum class ExecuteStatus : uint8_t {
  IDLE,
  TURN,
  ADVANCE,
  SETTLE,
  PAUSED,
  DONE,
  NO_PATH,
  BLOCKED,
  FAULT,
  FALLBACK,
  TIMEOUT,
  HALTED
};
struct DriveRequest {
  float linear_m_s = 0, angular_rad_s = 0;
  ExecuteStatus status = ExecuteStatus::IDLE;
  unsigned waypoint = 0;
};
struct ExecutorConfig {
  float max_speed_m_s = 0.04f, max_turn_rad_s = 0.5f;
  float position_tolerance_m = 0.025f, heading_tolerance_rad = loc::deg2rad(5);
  float obstacle_stop_m = 0.025f, settle_gyro_rad_s = 0.05f,
        settle_pose_step_m = 0.005f;
  int64_t coast_settle_us =
      400000; // Zero PWM dwell, not measured wheel standstill.
  int64_t leg_timeout_us = 30000000;
};
class PathExecutor {
public:
  /** @brief Initialize the portable cardinal waypoint executor.
   * @param[in] config Speed, arrival and timeout settings for the actual robot.
   * @returns No value.
   */
  explicit PathExecutor(const ExecutorConfig &config = ExecutorConfig{})
      : cfg_(config) {}
  /** @brief Plan a route from a trusted pose with the resolved map.
   * @param[in] map Current physical wall map.
   * @param[in] goal World cell in 0..4.
   * @param[in] pose Fresh tracked starting pose.
   * @param[in] now_us Shared monotonic clock.
   * @returns true if a complete path exists, false otherwise; output remains
   * zero.
   */
  bool begin(const loc::MazeMap &map, loc::Cell goal,
             const loc::PoseSnapshot &pose, int64_t now_us);
  /** @brief Compute one stop/turn/advance/settle request without touching
   * hardware.
   * @param[in] pose Latest localization snapshot.
   * @param[in] motor Latest applied-PWM request/status snapshot; no wheel
   * feedback.
   * @param[in] sensors Latest sensor frame for fresh front-range obstacle
   * checking.
   * @param[in] now_us Shared monotonic clock.
   * @param[in] heading Optional independent fresh heading for a
   * translation-unobservable pivot. Never authorizes forward motion or settled
   * arrival without a fresh pose.
   * @returns Nominal body velocity request; zero when stale, blocked, done or
   * faulted.
   * @note Phase changes require continuous zero PWM dwell, fresh quiet gyro and
   *       stable fresh poses. This cannot detect wheel slip, stall or straight
   *       coasting that produces no yaw; calibrate dwell on the actual robot.
   */
  DriveRequest update(const loc::PoseSnapshot &pose,
                      const motor::MotorSnapshot &motor,
                      const BootstrapFrame &sensors, int64_t now_us,
                      const HeadingObservation *heading = nullptr);
  /** @brief Cancel a route and clear its pending commands.
   * @returns No value.
   */
  void cancel();
  /** @brief Inspect the current source-to-goal route.
   * @returns Fixed-capacity path, possibly empty.
   */
  const AStar::CoordinateList &path() const { return path_; }

private:
  ExecutorConfig cfg_{};
  loc::MazeMap map_{};
  AStar::CoordinateList path_{};
  ExecuteStatus phase_ = ExecuteStatus::IDLE;
  unsigned index_ = 0, stable_ = 0;
  int64_t leg_start_ = 0, last_pose_ = 0, last_motor_ = 0, zero_since_ = 0;
  float last_x_ = 0, last_y_ = 0;
};
} // namespace nav
