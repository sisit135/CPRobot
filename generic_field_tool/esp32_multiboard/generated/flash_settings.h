/*
 *  This file is generated with Embedded Proto, PLEASE DO NOT EDIT!
 *  source: flash_settings.proto
 */

// This file is generated. Please do not edit!
#ifndef FLASH_SETTINGS_H
#define FLASH_SETTINGS_H

#include <cstdint>
#include <MessageInterface.h>
#include <WireFormatter.h>
#include <Fields.h>
#include <MessageSizeCalculator.h>
#include <ReadBufferSection.h>
#include <RepeatedFieldFixedSize.h>
#include <FieldStringBytes.h>
#include <Errors.h>
#include <Defines.h>
#include <limits>

// Include external proto definitions

namespace axi {
namespace remote {
namespace v1 {

class FlashSettings final: public ::EmbeddedProto::MessageInterface
{
  public:
    FlashSettings() = default;
    FlashSettings(const FlashSettings& rhs )
    {
      set_legacy_color_0_enabled(rhs.get_legacy_color_0_enabled());
      set_legacy_color_0_red(rhs.get_legacy_color_0_red());
      set_legacy_color_0_green(rhs.get_legacy_color_0_green());
      set_legacy_color_0_blue(rhs.get_legacy_color_0_blue());
      set_legacy_color_0_geometry(rhs.get_legacy_color_0_geometry());
      set_legacy_color_0_gx(rhs.get_legacy_color_0_gx());
      set_legacy_color_0_gy(rhs.get_legacy_color_0_gy());
      set_legacy_color_1_enabled(rhs.get_legacy_color_1_enabled());
      set_legacy_color_1_red(rhs.get_legacy_color_1_red());
      set_legacy_color_1_green(rhs.get_legacy_color_1_green());
      set_legacy_color_1_blue(rhs.get_legacy_color_1_blue());
      set_legacy_color_1_geometry(rhs.get_legacy_color_1_geometry());
      set_legacy_color_1_gx(rhs.get_legacy_color_1_gx());
      set_legacy_color_1_gy(rhs.get_legacy_color_1_gy());
      set_legacy_color_2_enabled(rhs.get_legacy_color_2_enabled());
      set_legacy_color_2_red(rhs.get_legacy_color_2_red());
      set_legacy_color_2_green(rhs.get_legacy_color_2_green());
      set_legacy_color_2_blue(rhs.get_legacy_color_2_blue());
      set_legacy_color_2_geometry(rhs.get_legacy_color_2_geometry());
      set_legacy_color_2_gx(rhs.get_legacy_color_2_gx());
      set_legacy_color_2_gy(rhs.get_legacy_color_2_gy());
      set_legacy_color_3_enabled(rhs.get_legacy_color_3_enabled());
      set_legacy_color_3_red(rhs.get_legacy_color_3_red());
      set_legacy_color_3_green(rhs.get_legacy_color_3_green());
      set_legacy_color_3_blue(rhs.get_legacy_color_3_blue());
      set_legacy_color_3_geometry(rhs.get_legacy_color_3_geometry());
      set_legacy_color_3_gx(rhs.get_legacy_color_3_gx());
      set_legacy_color_3_gy(rhs.get_legacy_color_3_gy());
      set_feature_variant_mask(rhs.get_feature_variant_mask());
      set_feature_sigma_m(rhs.get_feature_sigma_m());
      set_feature_max_score(rhs.get_feature_max_score());
      set_feature_min_margin(rhs.get_feature_min_margin());
      set_feature_max_translation_m_s(rhs.get_feature_max_translation_m_s());
      set_feature_confirmations(rhs.get_feature_confirmations());
      set_feature_contradiction_limit(rhs.get_feature_contradiction_limit());
      set_feature_max_age_us(rhs.get_feature_max_age_us());
      set_feature_max_skew_us(rhs.get_feature_max_skew_us());
      set_feature_coast_allowance_m(rhs.get_feature_coast_allowance_m());
      set_feature_command_latency_s(rhs.get_feature_command_latency_s());
      set_rhr_speed_m_s(rhs.get_rhr_speed_m_s());
      set_rhr_turn_rad_s(rhs.get_rhr_turn_rad_s());
      set_rhr_wall_distance_m(rhs.get_rhr_wall_distance_m());
      set_rhr_front_stop_m(rhs.get_rhr_front_stop_m());
      set_rhr_side_stop_m(rhs.get_rhr_side_stop_m());
      set_rhr_opening_m(rhs.get_rhr_opening_m());
      set_rhr_wall_gain(rhs.get_rhr_wall_gain());
      set_rhr_max_wall_turn_rad_s(rhs.get_rhr_max_wall_turn_rad_s());
      set_rhr_range_max_age_us(rhs.get_rhr_range_max_age_us());
      set_rhr_imu_max_age_us(rhs.get_rhr_imu_max_age_us());
      set_rhr_turn_timeout_us(rhs.get_rhr_turn_timeout_us());
      set_rhr_coast_us(rhs.get_rhr_coast_us());
      set_rhr_clear_us(rhs.get_rhr_clear_us());
      set_tape_black_max(rhs.get_tape_black_max());
      set_tape_white_min(rhs.get_tape_white_min());
      set_gyro_zero_rps(rhs.get_gyro_zero_rps());
    }

    FlashSettings(const FlashSettings&& rhs ) noexcept
    {
      set_legacy_color_0_enabled(rhs.get_legacy_color_0_enabled());
      set_legacy_color_0_red(rhs.get_legacy_color_0_red());
      set_legacy_color_0_green(rhs.get_legacy_color_0_green());
      set_legacy_color_0_blue(rhs.get_legacy_color_0_blue());
      set_legacy_color_0_geometry(rhs.get_legacy_color_0_geometry());
      set_legacy_color_0_gx(rhs.get_legacy_color_0_gx());
      set_legacy_color_0_gy(rhs.get_legacy_color_0_gy());
      set_legacy_color_1_enabled(rhs.get_legacy_color_1_enabled());
      set_legacy_color_1_red(rhs.get_legacy_color_1_red());
      set_legacy_color_1_green(rhs.get_legacy_color_1_green());
      set_legacy_color_1_blue(rhs.get_legacy_color_1_blue());
      set_legacy_color_1_geometry(rhs.get_legacy_color_1_geometry());
      set_legacy_color_1_gx(rhs.get_legacy_color_1_gx());
      set_legacy_color_1_gy(rhs.get_legacy_color_1_gy());
      set_legacy_color_2_enabled(rhs.get_legacy_color_2_enabled());
      set_legacy_color_2_red(rhs.get_legacy_color_2_red());
      set_legacy_color_2_green(rhs.get_legacy_color_2_green());
      set_legacy_color_2_blue(rhs.get_legacy_color_2_blue());
      set_legacy_color_2_geometry(rhs.get_legacy_color_2_geometry());
      set_legacy_color_2_gx(rhs.get_legacy_color_2_gx());
      set_legacy_color_2_gy(rhs.get_legacy_color_2_gy());
      set_legacy_color_3_enabled(rhs.get_legacy_color_3_enabled());
      set_legacy_color_3_red(rhs.get_legacy_color_3_red());
      set_legacy_color_3_green(rhs.get_legacy_color_3_green());
      set_legacy_color_3_blue(rhs.get_legacy_color_3_blue());
      set_legacy_color_3_geometry(rhs.get_legacy_color_3_geometry());
      set_legacy_color_3_gx(rhs.get_legacy_color_3_gx());
      set_legacy_color_3_gy(rhs.get_legacy_color_3_gy());
      set_feature_variant_mask(rhs.get_feature_variant_mask());
      set_feature_sigma_m(rhs.get_feature_sigma_m());
      set_feature_max_score(rhs.get_feature_max_score());
      set_feature_min_margin(rhs.get_feature_min_margin());
      set_feature_max_translation_m_s(rhs.get_feature_max_translation_m_s());
      set_feature_confirmations(rhs.get_feature_confirmations());
      set_feature_contradiction_limit(rhs.get_feature_contradiction_limit());
      set_feature_max_age_us(rhs.get_feature_max_age_us());
      set_feature_max_skew_us(rhs.get_feature_max_skew_us());
      set_feature_coast_allowance_m(rhs.get_feature_coast_allowance_m());
      set_feature_command_latency_s(rhs.get_feature_command_latency_s());
      set_rhr_speed_m_s(rhs.get_rhr_speed_m_s());
      set_rhr_turn_rad_s(rhs.get_rhr_turn_rad_s());
      set_rhr_wall_distance_m(rhs.get_rhr_wall_distance_m());
      set_rhr_front_stop_m(rhs.get_rhr_front_stop_m());
      set_rhr_side_stop_m(rhs.get_rhr_side_stop_m());
      set_rhr_opening_m(rhs.get_rhr_opening_m());
      set_rhr_wall_gain(rhs.get_rhr_wall_gain());
      set_rhr_max_wall_turn_rad_s(rhs.get_rhr_max_wall_turn_rad_s());
      set_rhr_range_max_age_us(rhs.get_rhr_range_max_age_us());
      set_rhr_imu_max_age_us(rhs.get_rhr_imu_max_age_us());
      set_rhr_turn_timeout_us(rhs.get_rhr_turn_timeout_us());
      set_rhr_coast_us(rhs.get_rhr_coast_us());
      set_rhr_clear_us(rhs.get_rhr_clear_us());
      set_tape_black_max(rhs.get_tape_black_max());
      set_tape_white_min(rhs.get_tape_white_min());
      set_gyro_zero_rps(rhs.get_gyro_zero_rps());
    }

    ~FlashSettings() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      LEGACY_COLOR_0_ENABLED = 100,
      LEGACY_COLOR_0_RED = 101,
      LEGACY_COLOR_0_GREEN = 102,
      LEGACY_COLOR_0_BLUE = 103,
      LEGACY_COLOR_0_GEOMETRY = 104,
      LEGACY_COLOR_0_GX = 105,
      LEGACY_COLOR_0_GY = 106,
      LEGACY_COLOR_1_ENABLED = 110,
      LEGACY_COLOR_1_RED = 111,
      LEGACY_COLOR_1_GREEN = 112,
      LEGACY_COLOR_1_BLUE = 113,
      LEGACY_COLOR_1_GEOMETRY = 114,
      LEGACY_COLOR_1_GX = 115,
      LEGACY_COLOR_1_GY = 116,
      LEGACY_COLOR_2_ENABLED = 120,
      LEGACY_COLOR_2_RED = 121,
      LEGACY_COLOR_2_GREEN = 122,
      LEGACY_COLOR_2_BLUE = 123,
      LEGACY_COLOR_2_GEOMETRY = 124,
      LEGACY_COLOR_2_GX = 125,
      LEGACY_COLOR_2_GY = 126,
      LEGACY_COLOR_3_ENABLED = 130,
      LEGACY_COLOR_3_RED = 131,
      LEGACY_COLOR_3_GREEN = 132,
      LEGACY_COLOR_3_BLUE = 133,
      LEGACY_COLOR_3_GEOMETRY = 134,
      LEGACY_COLOR_3_GX = 135,
      LEGACY_COLOR_3_GY = 136,
      FEATURE_VARIANT_MASK = 200,
      FEATURE_SIGMA_M = 201,
      FEATURE_MAX_SCORE = 202,
      FEATURE_MIN_MARGIN = 203,
      FEATURE_MAX_TRANSLATION_M_S = 204,
      FEATURE_CONFIRMATIONS = 205,
      FEATURE_CONTRADICTION_LIMIT = 206,
      FEATURE_MAX_AGE_US = 207,
      FEATURE_MAX_SKEW_US = 208,
      FEATURE_COAST_ALLOWANCE_M = 209,
      FEATURE_COMMAND_LATENCY_S = 210,
      RHR_SPEED_M_S = 220,
      RHR_TURN_RAD_S = 221,
      RHR_WALL_DISTANCE_M = 222,
      RHR_FRONT_STOP_M = 223,
      RHR_SIDE_STOP_M = 224,
      RHR_OPENING_M = 225,
      RHR_WALL_GAIN = 226,
      RHR_MAX_WALL_TURN_RAD_S = 227,
      RHR_RANGE_MAX_AGE_US = 228,
      RHR_IMU_MAX_AGE_US = 229,
      RHR_TURN_TIMEOUT_US = 230,
      RHR_COAST_US = 231,
      RHR_CLEAR_US = 232,
      TAPE_BLACK_MAX = 240,
      TAPE_WHITE_MIN = 241,
      GYRO_ZERO_RPS = 250
    };

    FlashSettings& operator=(const FlashSettings& rhs)
    {
      set_legacy_color_0_enabled(rhs.get_legacy_color_0_enabled());
      set_legacy_color_0_red(rhs.get_legacy_color_0_red());
      set_legacy_color_0_green(rhs.get_legacy_color_0_green());
      set_legacy_color_0_blue(rhs.get_legacy_color_0_blue());
      set_legacy_color_0_geometry(rhs.get_legacy_color_0_geometry());
      set_legacy_color_0_gx(rhs.get_legacy_color_0_gx());
      set_legacy_color_0_gy(rhs.get_legacy_color_0_gy());
      set_legacy_color_1_enabled(rhs.get_legacy_color_1_enabled());
      set_legacy_color_1_red(rhs.get_legacy_color_1_red());
      set_legacy_color_1_green(rhs.get_legacy_color_1_green());
      set_legacy_color_1_blue(rhs.get_legacy_color_1_blue());
      set_legacy_color_1_geometry(rhs.get_legacy_color_1_geometry());
      set_legacy_color_1_gx(rhs.get_legacy_color_1_gx());
      set_legacy_color_1_gy(rhs.get_legacy_color_1_gy());
      set_legacy_color_2_enabled(rhs.get_legacy_color_2_enabled());
      set_legacy_color_2_red(rhs.get_legacy_color_2_red());
      set_legacy_color_2_green(rhs.get_legacy_color_2_green());
      set_legacy_color_2_blue(rhs.get_legacy_color_2_blue());
      set_legacy_color_2_geometry(rhs.get_legacy_color_2_geometry());
      set_legacy_color_2_gx(rhs.get_legacy_color_2_gx());
      set_legacy_color_2_gy(rhs.get_legacy_color_2_gy());
      set_legacy_color_3_enabled(rhs.get_legacy_color_3_enabled());
      set_legacy_color_3_red(rhs.get_legacy_color_3_red());
      set_legacy_color_3_green(rhs.get_legacy_color_3_green());
      set_legacy_color_3_blue(rhs.get_legacy_color_3_blue());
      set_legacy_color_3_geometry(rhs.get_legacy_color_3_geometry());
      set_legacy_color_3_gx(rhs.get_legacy_color_3_gx());
      set_legacy_color_3_gy(rhs.get_legacy_color_3_gy());
      set_feature_variant_mask(rhs.get_feature_variant_mask());
      set_feature_sigma_m(rhs.get_feature_sigma_m());
      set_feature_max_score(rhs.get_feature_max_score());
      set_feature_min_margin(rhs.get_feature_min_margin());
      set_feature_max_translation_m_s(rhs.get_feature_max_translation_m_s());
      set_feature_confirmations(rhs.get_feature_confirmations());
      set_feature_contradiction_limit(rhs.get_feature_contradiction_limit());
      set_feature_max_age_us(rhs.get_feature_max_age_us());
      set_feature_max_skew_us(rhs.get_feature_max_skew_us());
      set_feature_coast_allowance_m(rhs.get_feature_coast_allowance_m());
      set_feature_command_latency_s(rhs.get_feature_command_latency_s());
      set_rhr_speed_m_s(rhs.get_rhr_speed_m_s());
      set_rhr_turn_rad_s(rhs.get_rhr_turn_rad_s());
      set_rhr_wall_distance_m(rhs.get_rhr_wall_distance_m());
      set_rhr_front_stop_m(rhs.get_rhr_front_stop_m());
      set_rhr_side_stop_m(rhs.get_rhr_side_stop_m());
      set_rhr_opening_m(rhs.get_rhr_opening_m());
      set_rhr_wall_gain(rhs.get_rhr_wall_gain());
      set_rhr_max_wall_turn_rad_s(rhs.get_rhr_max_wall_turn_rad_s());
      set_rhr_range_max_age_us(rhs.get_rhr_range_max_age_us());
      set_rhr_imu_max_age_us(rhs.get_rhr_imu_max_age_us());
      set_rhr_turn_timeout_us(rhs.get_rhr_turn_timeout_us());
      set_rhr_coast_us(rhs.get_rhr_coast_us());
      set_rhr_clear_us(rhs.get_rhr_clear_us());
      set_tape_black_max(rhs.get_tape_black_max());
      set_tape_white_min(rhs.get_tape_white_min());
      set_gyro_zero_rps(rhs.get_gyro_zero_rps());
      return *this;
    }

    FlashSettings& operator=(const FlashSettings&& rhs) noexcept
    {
      set_legacy_color_0_enabled(rhs.get_legacy_color_0_enabled());
      set_legacy_color_0_red(rhs.get_legacy_color_0_red());
      set_legacy_color_0_green(rhs.get_legacy_color_0_green());
      set_legacy_color_0_blue(rhs.get_legacy_color_0_blue());
      set_legacy_color_0_geometry(rhs.get_legacy_color_0_geometry());
      set_legacy_color_0_gx(rhs.get_legacy_color_0_gx());
      set_legacy_color_0_gy(rhs.get_legacy_color_0_gy());
      set_legacy_color_1_enabled(rhs.get_legacy_color_1_enabled());
      set_legacy_color_1_red(rhs.get_legacy_color_1_red());
      set_legacy_color_1_green(rhs.get_legacy_color_1_green());
      set_legacy_color_1_blue(rhs.get_legacy_color_1_blue());
      set_legacy_color_1_geometry(rhs.get_legacy_color_1_geometry());
      set_legacy_color_1_gx(rhs.get_legacy_color_1_gx());
      set_legacy_color_1_gy(rhs.get_legacy_color_1_gy());
      set_legacy_color_2_enabled(rhs.get_legacy_color_2_enabled());
      set_legacy_color_2_red(rhs.get_legacy_color_2_red());
      set_legacy_color_2_green(rhs.get_legacy_color_2_green());
      set_legacy_color_2_blue(rhs.get_legacy_color_2_blue());
      set_legacy_color_2_geometry(rhs.get_legacy_color_2_geometry());
      set_legacy_color_2_gx(rhs.get_legacy_color_2_gx());
      set_legacy_color_2_gy(rhs.get_legacy_color_2_gy());
      set_legacy_color_3_enabled(rhs.get_legacy_color_3_enabled());
      set_legacy_color_3_red(rhs.get_legacy_color_3_red());
      set_legacy_color_3_green(rhs.get_legacy_color_3_green());
      set_legacy_color_3_blue(rhs.get_legacy_color_3_blue());
      set_legacy_color_3_geometry(rhs.get_legacy_color_3_geometry());
      set_legacy_color_3_gx(rhs.get_legacy_color_3_gx());
      set_legacy_color_3_gy(rhs.get_legacy_color_3_gy());
      set_feature_variant_mask(rhs.get_feature_variant_mask());
      set_feature_sigma_m(rhs.get_feature_sigma_m());
      set_feature_max_score(rhs.get_feature_max_score());
      set_feature_min_margin(rhs.get_feature_min_margin());
      set_feature_max_translation_m_s(rhs.get_feature_max_translation_m_s());
      set_feature_confirmations(rhs.get_feature_confirmations());
      set_feature_contradiction_limit(rhs.get_feature_contradiction_limit());
      set_feature_max_age_us(rhs.get_feature_max_age_us());
      set_feature_max_skew_us(rhs.get_feature_max_skew_us());
      set_feature_coast_allowance_m(rhs.get_feature_coast_allowance_m());
      set_feature_command_latency_s(rhs.get_feature_command_latency_s());
      set_rhr_speed_m_s(rhs.get_rhr_speed_m_s());
      set_rhr_turn_rad_s(rhs.get_rhr_turn_rad_s());
      set_rhr_wall_distance_m(rhs.get_rhr_wall_distance_m());
      set_rhr_front_stop_m(rhs.get_rhr_front_stop_m());
      set_rhr_side_stop_m(rhs.get_rhr_side_stop_m());
      set_rhr_opening_m(rhs.get_rhr_opening_m());
      set_rhr_wall_gain(rhs.get_rhr_wall_gain());
      set_rhr_max_wall_turn_rad_s(rhs.get_rhr_max_wall_turn_rad_s());
      set_rhr_range_max_age_us(rhs.get_rhr_range_max_age_us());
      set_rhr_imu_max_age_us(rhs.get_rhr_imu_max_age_us());
      set_rhr_turn_timeout_us(rhs.get_rhr_turn_timeout_us());
      set_rhr_coast_us(rhs.get_rhr_coast_us());
      set_rhr_clear_us(rhs.get_rhr_clear_us());
      set_tape_black_max(rhs.get_tape_black_max());
      set_tape_white_min(rhs.get_tape_white_min());
      set_gyro_zero_rps(rhs.get_gyro_zero_rps());
      return *this;
    }

    static constexpr char const* LEGACY_COLOR_0_ENABLED_NAME = "legacy_color_0_enabled";
    inline void clear_legacy_color_0_enabled() { legacy_color_0_enabled_.clear(); }
    inline void set_legacy_color_0_enabled(const bool& value) { legacy_color_0_enabled_ = value; }
    inline void set_legacy_color_0_enabled(const bool&& value) { legacy_color_0_enabled_ = value; }
    inline bool& mutable_legacy_color_0_enabled() { return legacy_color_0_enabled_.get(); }
    inline const bool& get_legacy_color_0_enabled() const { return legacy_color_0_enabled_.get(); }
    inline bool legacy_color_0_enabled() const { return legacy_color_0_enabled_.get(); }

    static constexpr char const* LEGACY_COLOR_0_RED_NAME = "legacy_color_0_red";
    inline void clear_legacy_color_0_red() { legacy_color_0_red_.clear(); }
    inline void set_legacy_color_0_red(const float& value) { legacy_color_0_red_ = value; }
    inline void set_legacy_color_0_red(const float&& value) { legacy_color_0_red_ = value; }
    inline float& mutable_legacy_color_0_red() { return legacy_color_0_red_.get(); }
    inline const float& get_legacy_color_0_red() const { return legacy_color_0_red_.get(); }
    inline float legacy_color_0_red() const { return legacy_color_0_red_.get(); }

    static constexpr char const* LEGACY_COLOR_0_GREEN_NAME = "legacy_color_0_green";
    inline void clear_legacy_color_0_green() { legacy_color_0_green_.clear(); }
    inline void set_legacy_color_0_green(const float& value) { legacy_color_0_green_ = value; }
    inline void set_legacy_color_0_green(const float&& value) { legacy_color_0_green_ = value; }
    inline float& mutable_legacy_color_0_green() { return legacy_color_0_green_.get(); }
    inline const float& get_legacy_color_0_green() const { return legacy_color_0_green_.get(); }
    inline float legacy_color_0_green() const { return legacy_color_0_green_.get(); }

    static constexpr char const* LEGACY_COLOR_0_BLUE_NAME = "legacy_color_0_blue";
    inline void clear_legacy_color_0_blue() { legacy_color_0_blue_.clear(); }
    inline void set_legacy_color_0_blue(const float& value) { legacy_color_0_blue_ = value; }
    inline void set_legacy_color_0_blue(const float&& value) { legacy_color_0_blue_ = value; }
    inline float& mutable_legacy_color_0_blue() { return legacy_color_0_blue_.get(); }
    inline const float& get_legacy_color_0_blue() const { return legacy_color_0_blue_.get(); }
    inline float legacy_color_0_blue() const { return legacy_color_0_blue_.get(); }

    static constexpr char const* LEGACY_COLOR_0_GEOMETRY_NAME = "legacy_color_0_geometry";
    inline void clear_legacy_color_0_geometry() { legacy_color_0_geometry_.clear(); }
    inline void set_legacy_color_0_geometry(const uint32_t& value) { legacy_color_0_geometry_ = value; }
    inline void set_legacy_color_0_geometry(const uint32_t&& value) { legacy_color_0_geometry_ = value; }
    inline uint32_t& mutable_legacy_color_0_geometry() { return legacy_color_0_geometry_.get(); }
    inline const uint32_t& get_legacy_color_0_geometry() const { return legacy_color_0_geometry_.get(); }
    inline uint32_t legacy_color_0_geometry() const { return legacy_color_0_geometry_.get(); }

    static constexpr char const* LEGACY_COLOR_0_GX_NAME = "legacy_color_0_gx";
    inline void clear_legacy_color_0_gx() { legacy_color_0_gx_.clear(); }
    inline void set_legacy_color_0_gx(const uint32_t& value) { legacy_color_0_gx_ = value; }
    inline void set_legacy_color_0_gx(const uint32_t&& value) { legacy_color_0_gx_ = value; }
    inline uint32_t& mutable_legacy_color_0_gx() { return legacy_color_0_gx_.get(); }
    inline const uint32_t& get_legacy_color_0_gx() const { return legacy_color_0_gx_.get(); }
    inline uint32_t legacy_color_0_gx() const { return legacy_color_0_gx_.get(); }

    static constexpr char const* LEGACY_COLOR_0_GY_NAME = "legacy_color_0_gy";
    inline void clear_legacy_color_0_gy() { legacy_color_0_gy_.clear(); }
    inline void set_legacy_color_0_gy(const uint32_t& value) { legacy_color_0_gy_ = value; }
    inline void set_legacy_color_0_gy(const uint32_t&& value) { legacy_color_0_gy_ = value; }
    inline uint32_t& mutable_legacy_color_0_gy() { return legacy_color_0_gy_.get(); }
    inline const uint32_t& get_legacy_color_0_gy() const { return legacy_color_0_gy_.get(); }
    inline uint32_t legacy_color_0_gy() const { return legacy_color_0_gy_.get(); }

    static constexpr char const* LEGACY_COLOR_1_ENABLED_NAME = "legacy_color_1_enabled";
    inline void clear_legacy_color_1_enabled() { legacy_color_1_enabled_.clear(); }
    inline void set_legacy_color_1_enabled(const bool& value) { legacy_color_1_enabled_ = value; }
    inline void set_legacy_color_1_enabled(const bool&& value) { legacy_color_1_enabled_ = value; }
    inline bool& mutable_legacy_color_1_enabled() { return legacy_color_1_enabled_.get(); }
    inline const bool& get_legacy_color_1_enabled() const { return legacy_color_1_enabled_.get(); }
    inline bool legacy_color_1_enabled() const { return legacy_color_1_enabled_.get(); }

    static constexpr char const* LEGACY_COLOR_1_RED_NAME = "legacy_color_1_red";
    inline void clear_legacy_color_1_red() { legacy_color_1_red_.clear(); }
    inline void set_legacy_color_1_red(const float& value) { legacy_color_1_red_ = value; }
    inline void set_legacy_color_1_red(const float&& value) { legacy_color_1_red_ = value; }
    inline float& mutable_legacy_color_1_red() { return legacy_color_1_red_.get(); }
    inline const float& get_legacy_color_1_red() const { return legacy_color_1_red_.get(); }
    inline float legacy_color_1_red() const { return legacy_color_1_red_.get(); }

    static constexpr char const* LEGACY_COLOR_1_GREEN_NAME = "legacy_color_1_green";
    inline void clear_legacy_color_1_green() { legacy_color_1_green_.clear(); }
    inline void set_legacy_color_1_green(const float& value) { legacy_color_1_green_ = value; }
    inline void set_legacy_color_1_green(const float&& value) { legacy_color_1_green_ = value; }
    inline float& mutable_legacy_color_1_green() { return legacy_color_1_green_.get(); }
    inline const float& get_legacy_color_1_green() const { return legacy_color_1_green_.get(); }
    inline float legacy_color_1_green() const { return legacy_color_1_green_.get(); }

    static constexpr char const* LEGACY_COLOR_1_BLUE_NAME = "legacy_color_1_blue";
    inline void clear_legacy_color_1_blue() { legacy_color_1_blue_.clear(); }
    inline void set_legacy_color_1_blue(const float& value) { legacy_color_1_blue_ = value; }
    inline void set_legacy_color_1_blue(const float&& value) { legacy_color_1_blue_ = value; }
    inline float& mutable_legacy_color_1_blue() { return legacy_color_1_blue_.get(); }
    inline const float& get_legacy_color_1_blue() const { return legacy_color_1_blue_.get(); }
    inline float legacy_color_1_blue() const { return legacy_color_1_blue_.get(); }

    static constexpr char const* LEGACY_COLOR_1_GEOMETRY_NAME = "legacy_color_1_geometry";
    inline void clear_legacy_color_1_geometry() { legacy_color_1_geometry_.clear(); }
    inline void set_legacy_color_1_geometry(const uint32_t& value) { legacy_color_1_geometry_ = value; }
    inline void set_legacy_color_1_geometry(const uint32_t&& value) { legacy_color_1_geometry_ = value; }
    inline uint32_t& mutable_legacy_color_1_geometry() { return legacy_color_1_geometry_.get(); }
    inline const uint32_t& get_legacy_color_1_geometry() const { return legacy_color_1_geometry_.get(); }
    inline uint32_t legacy_color_1_geometry() const { return legacy_color_1_geometry_.get(); }

    static constexpr char const* LEGACY_COLOR_1_GX_NAME = "legacy_color_1_gx";
    inline void clear_legacy_color_1_gx() { legacy_color_1_gx_.clear(); }
    inline void set_legacy_color_1_gx(const uint32_t& value) { legacy_color_1_gx_ = value; }
    inline void set_legacy_color_1_gx(const uint32_t&& value) { legacy_color_1_gx_ = value; }
    inline uint32_t& mutable_legacy_color_1_gx() { return legacy_color_1_gx_.get(); }
    inline const uint32_t& get_legacy_color_1_gx() const { return legacy_color_1_gx_.get(); }
    inline uint32_t legacy_color_1_gx() const { return legacy_color_1_gx_.get(); }

    static constexpr char const* LEGACY_COLOR_1_GY_NAME = "legacy_color_1_gy";
    inline void clear_legacy_color_1_gy() { legacy_color_1_gy_.clear(); }
    inline void set_legacy_color_1_gy(const uint32_t& value) { legacy_color_1_gy_ = value; }
    inline void set_legacy_color_1_gy(const uint32_t&& value) { legacy_color_1_gy_ = value; }
    inline uint32_t& mutable_legacy_color_1_gy() { return legacy_color_1_gy_.get(); }
    inline const uint32_t& get_legacy_color_1_gy() const { return legacy_color_1_gy_.get(); }
    inline uint32_t legacy_color_1_gy() const { return legacy_color_1_gy_.get(); }

    static constexpr char const* LEGACY_COLOR_2_ENABLED_NAME = "legacy_color_2_enabled";
    inline void clear_legacy_color_2_enabled() { legacy_color_2_enabled_.clear(); }
    inline void set_legacy_color_2_enabled(const bool& value) { legacy_color_2_enabled_ = value; }
    inline void set_legacy_color_2_enabled(const bool&& value) { legacy_color_2_enabled_ = value; }
    inline bool& mutable_legacy_color_2_enabled() { return legacy_color_2_enabled_.get(); }
    inline const bool& get_legacy_color_2_enabled() const { return legacy_color_2_enabled_.get(); }
    inline bool legacy_color_2_enabled() const { return legacy_color_2_enabled_.get(); }

    static constexpr char const* LEGACY_COLOR_2_RED_NAME = "legacy_color_2_red";
    inline void clear_legacy_color_2_red() { legacy_color_2_red_.clear(); }
    inline void set_legacy_color_2_red(const float& value) { legacy_color_2_red_ = value; }
    inline void set_legacy_color_2_red(const float&& value) { legacy_color_2_red_ = value; }
    inline float& mutable_legacy_color_2_red() { return legacy_color_2_red_.get(); }
    inline const float& get_legacy_color_2_red() const { return legacy_color_2_red_.get(); }
    inline float legacy_color_2_red() const { return legacy_color_2_red_.get(); }

    static constexpr char const* LEGACY_COLOR_2_GREEN_NAME = "legacy_color_2_green";
    inline void clear_legacy_color_2_green() { legacy_color_2_green_.clear(); }
    inline void set_legacy_color_2_green(const float& value) { legacy_color_2_green_ = value; }
    inline void set_legacy_color_2_green(const float&& value) { legacy_color_2_green_ = value; }
    inline float& mutable_legacy_color_2_green() { return legacy_color_2_green_.get(); }
    inline const float& get_legacy_color_2_green() const { return legacy_color_2_green_.get(); }
    inline float legacy_color_2_green() const { return legacy_color_2_green_.get(); }

    static constexpr char const* LEGACY_COLOR_2_BLUE_NAME = "legacy_color_2_blue";
    inline void clear_legacy_color_2_blue() { legacy_color_2_blue_.clear(); }
    inline void set_legacy_color_2_blue(const float& value) { legacy_color_2_blue_ = value; }
    inline void set_legacy_color_2_blue(const float&& value) { legacy_color_2_blue_ = value; }
    inline float& mutable_legacy_color_2_blue() { return legacy_color_2_blue_.get(); }
    inline const float& get_legacy_color_2_blue() const { return legacy_color_2_blue_.get(); }
    inline float legacy_color_2_blue() const { return legacy_color_2_blue_.get(); }

    static constexpr char const* LEGACY_COLOR_2_GEOMETRY_NAME = "legacy_color_2_geometry";
    inline void clear_legacy_color_2_geometry() { legacy_color_2_geometry_.clear(); }
    inline void set_legacy_color_2_geometry(const uint32_t& value) { legacy_color_2_geometry_ = value; }
    inline void set_legacy_color_2_geometry(const uint32_t&& value) { legacy_color_2_geometry_ = value; }
    inline uint32_t& mutable_legacy_color_2_geometry() { return legacy_color_2_geometry_.get(); }
    inline const uint32_t& get_legacy_color_2_geometry() const { return legacy_color_2_geometry_.get(); }
    inline uint32_t legacy_color_2_geometry() const { return legacy_color_2_geometry_.get(); }

    static constexpr char const* LEGACY_COLOR_2_GX_NAME = "legacy_color_2_gx";
    inline void clear_legacy_color_2_gx() { legacy_color_2_gx_.clear(); }
    inline void set_legacy_color_2_gx(const uint32_t& value) { legacy_color_2_gx_ = value; }
    inline void set_legacy_color_2_gx(const uint32_t&& value) { legacy_color_2_gx_ = value; }
    inline uint32_t& mutable_legacy_color_2_gx() { return legacy_color_2_gx_.get(); }
    inline const uint32_t& get_legacy_color_2_gx() const { return legacy_color_2_gx_.get(); }
    inline uint32_t legacy_color_2_gx() const { return legacy_color_2_gx_.get(); }

    static constexpr char const* LEGACY_COLOR_2_GY_NAME = "legacy_color_2_gy";
    inline void clear_legacy_color_2_gy() { legacy_color_2_gy_.clear(); }
    inline void set_legacy_color_2_gy(const uint32_t& value) { legacy_color_2_gy_ = value; }
    inline void set_legacy_color_2_gy(const uint32_t&& value) { legacy_color_2_gy_ = value; }
    inline uint32_t& mutable_legacy_color_2_gy() { return legacy_color_2_gy_.get(); }
    inline const uint32_t& get_legacy_color_2_gy() const { return legacy_color_2_gy_.get(); }
    inline uint32_t legacy_color_2_gy() const { return legacy_color_2_gy_.get(); }

    static constexpr char const* LEGACY_COLOR_3_ENABLED_NAME = "legacy_color_3_enabled";
    inline void clear_legacy_color_3_enabled() { legacy_color_3_enabled_.clear(); }
    inline void set_legacy_color_3_enabled(const bool& value) { legacy_color_3_enabled_ = value; }
    inline void set_legacy_color_3_enabled(const bool&& value) { legacy_color_3_enabled_ = value; }
    inline bool& mutable_legacy_color_3_enabled() { return legacy_color_3_enabled_.get(); }
    inline const bool& get_legacy_color_3_enabled() const { return legacy_color_3_enabled_.get(); }
    inline bool legacy_color_3_enabled() const { return legacy_color_3_enabled_.get(); }

    static constexpr char const* LEGACY_COLOR_3_RED_NAME = "legacy_color_3_red";
    inline void clear_legacy_color_3_red() { legacy_color_3_red_.clear(); }
    inline void set_legacy_color_3_red(const float& value) { legacy_color_3_red_ = value; }
    inline void set_legacy_color_3_red(const float&& value) { legacy_color_3_red_ = value; }
    inline float& mutable_legacy_color_3_red() { return legacy_color_3_red_.get(); }
    inline const float& get_legacy_color_3_red() const { return legacy_color_3_red_.get(); }
    inline float legacy_color_3_red() const { return legacy_color_3_red_.get(); }

    static constexpr char const* LEGACY_COLOR_3_GREEN_NAME = "legacy_color_3_green";
    inline void clear_legacy_color_3_green() { legacy_color_3_green_.clear(); }
    inline void set_legacy_color_3_green(const float& value) { legacy_color_3_green_ = value; }
    inline void set_legacy_color_3_green(const float&& value) { legacy_color_3_green_ = value; }
    inline float& mutable_legacy_color_3_green() { return legacy_color_3_green_.get(); }
    inline const float& get_legacy_color_3_green() const { return legacy_color_3_green_.get(); }
    inline float legacy_color_3_green() const { return legacy_color_3_green_.get(); }

    static constexpr char const* LEGACY_COLOR_3_BLUE_NAME = "legacy_color_3_blue";
    inline void clear_legacy_color_3_blue() { legacy_color_3_blue_.clear(); }
    inline void set_legacy_color_3_blue(const float& value) { legacy_color_3_blue_ = value; }
    inline void set_legacy_color_3_blue(const float&& value) { legacy_color_3_blue_ = value; }
    inline float& mutable_legacy_color_3_blue() { return legacy_color_3_blue_.get(); }
    inline const float& get_legacy_color_3_blue() const { return legacy_color_3_blue_.get(); }
    inline float legacy_color_3_blue() const { return legacy_color_3_blue_.get(); }

    static constexpr char const* LEGACY_COLOR_3_GEOMETRY_NAME = "legacy_color_3_geometry";
    inline void clear_legacy_color_3_geometry() { legacy_color_3_geometry_.clear(); }
    inline void set_legacy_color_3_geometry(const uint32_t& value) { legacy_color_3_geometry_ = value; }
    inline void set_legacy_color_3_geometry(const uint32_t&& value) { legacy_color_3_geometry_ = value; }
    inline uint32_t& mutable_legacy_color_3_geometry() { return legacy_color_3_geometry_.get(); }
    inline const uint32_t& get_legacy_color_3_geometry() const { return legacy_color_3_geometry_.get(); }
    inline uint32_t legacy_color_3_geometry() const { return legacy_color_3_geometry_.get(); }

    static constexpr char const* LEGACY_COLOR_3_GX_NAME = "legacy_color_3_gx";
    inline void clear_legacy_color_3_gx() { legacy_color_3_gx_.clear(); }
    inline void set_legacy_color_3_gx(const uint32_t& value) { legacy_color_3_gx_ = value; }
    inline void set_legacy_color_3_gx(const uint32_t&& value) { legacy_color_3_gx_ = value; }
    inline uint32_t& mutable_legacy_color_3_gx() { return legacy_color_3_gx_.get(); }
    inline const uint32_t& get_legacy_color_3_gx() const { return legacy_color_3_gx_.get(); }
    inline uint32_t legacy_color_3_gx() const { return legacy_color_3_gx_.get(); }

    static constexpr char const* LEGACY_COLOR_3_GY_NAME = "legacy_color_3_gy";
    inline void clear_legacy_color_3_gy() { legacy_color_3_gy_.clear(); }
    inline void set_legacy_color_3_gy(const uint32_t& value) { legacy_color_3_gy_ = value; }
    inline void set_legacy_color_3_gy(const uint32_t&& value) { legacy_color_3_gy_ = value; }
    inline uint32_t& mutable_legacy_color_3_gy() { return legacy_color_3_gy_.get(); }
    inline const uint32_t& get_legacy_color_3_gy() const { return legacy_color_3_gy_.get(); }
    inline uint32_t legacy_color_3_gy() const { return legacy_color_3_gy_.get(); }

    static constexpr char const* FEATURE_VARIANT_MASK_NAME = "feature_variant_mask";
    inline void clear_feature_variant_mask() { feature_variant_mask_.clear(); }
    inline void set_feature_variant_mask(const uint32_t& value) { feature_variant_mask_ = value; }
    inline void set_feature_variant_mask(const uint32_t&& value) { feature_variant_mask_ = value; }
    inline uint32_t& mutable_feature_variant_mask() { return feature_variant_mask_.get(); }
    inline const uint32_t& get_feature_variant_mask() const { return feature_variant_mask_.get(); }
    inline uint32_t feature_variant_mask() const { return feature_variant_mask_.get(); }

    static constexpr char const* FEATURE_SIGMA_M_NAME = "feature_sigma_m";
    inline void clear_feature_sigma_m() { feature_sigma_m_.clear(); }
    inline void set_feature_sigma_m(const float& value) { feature_sigma_m_ = value; }
    inline void set_feature_sigma_m(const float&& value) { feature_sigma_m_ = value; }
    inline float& mutable_feature_sigma_m() { return feature_sigma_m_.get(); }
    inline const float& get_feature_sigma_m() const { return feature_sigma_m_.get(); }
    inline float feature_sigma_m() const { return feature_sigma_m_.get(); }

    static constexpr char const* FEATURE_MAX_SCORE_NAME = "feature_max_score";
    inline void clear_feature_max_score() { feature_max_score_.clear(); }
    inline void set_feature_max_score(const float& value) { feature_max_score_ = value; }
    inline void set_feature_max_score(const float&& value) { feature_max_score_ = value; }
    inline float& mutable_feature_max_score() { return feature_max_score_.get(); }
    inline const float& get_feature_max_score() const { return feature_max_score_.get(); }
    inline float feature_max_score() const { return feature_max_score_.get(); }

    static constexpr char const* FEATURE_MIN_MARGIN_NAME = "feature_min_margin";
    inline void clear_feature_min_margin() { feature_min_margin_.clear(); }
    inline void set_feature_min_margin(const float& value) { feature_min_margin_ = value; }
    inline void set_feature_min_margin(const float&& value) { feature_min_margin_ = value; }
    inline float& mutable_feature_min_margin() { return feature_min_margin_.get(); }
    inline const float& get_feature_min_margin() const { return feature_min_margin_.get(); }
    inline float feature_min_margin() const { return feature_min_margin_.get(); }

    static constexpr char const* FEATURE_MAX_TRANSLATION_M_S_NAME = "feature_max_translation_m_s";
    inline void clear_feature_max_translation_m_s() { feature_max_translation_m_s_.clear(); }
    inline void set_feature_max_translation_m_s(const float& value) { feature_max_translation_m_s_ = value; }
    inline void set_feature_max_translation_m_s(const float&& value) { feature_max_translation_m_s_ = value; }
    inline float& mutable_feature_max_translation_m_s() { return feature_max_translation_m_s_.get(); }
    inline const float& get_feature_max_translation_m_s() const { return feature_max_translation_m_s_.get(); }
    inline float feature_max_translation_m_s() const { return feature_max_translation_m_s_.get(); }

    static constexpr char const* FEATURE_CONFIRMATIONS_NAME = "feature_confirmations";
    inline void clear_feature_confirmations() { feature_confirmations_.clear(); }
    inline void set_feature_confirmations(const uint32_t& value) { feature_confirmations_ = value; }
    inline void set_feature_confirmations(const uint32_t&& value) { feature_confirmations_ = value; }
    inline uint32_t& mutable_feature_confirmations() { return feature_confirmations_.get(); }
    inline const uint32_t& get_feature_confirmations() const { return feature_confirmations_.get(); }
    inline uint32_t feature_confirmations() const { return feature_confirmations_.get(); }

    static constexpr char const* FEATURE_CONTRADICTION_LIMIT_NAME = "feature_contradiction_limit";
    inline void clear_feature_contradiction_limit() { feature_contradiction_limit_.clear(); }
    inline void set_feature_contradiction_limit(const uint32_t& value) { feature_contradiction_limit_ = value; }
    inline void set_feature_contradiction_limit(const uint32_t&& value) { feature_contradiction_limit_ = value; }
    inline uint32_t& mutable_feature_contradiction_limit() { return feature_contradiction_limit_.get(); }
    inline const uint32_t& get_feature_contradiction_limit() const { return feature_contradiction_limit_.get(); }
    inline uint32_t feature_contradiction_limit() const { return feature_contradiction_limit_.get(); }

    static constexpr char const* FEATURE_MAX_AGE_US_NAME = "feature_max_age_us";
    inline void clear_feature_max_age_us() { feature_max_age_us_.clear(); }
    inline void set_feature_max_age_us(const uint32_t& value) { feature_max_age_us_ = value; }
    inline void set_feature_max_age_us(const uint32_t&& value) { feature_max_age_us_ = value; }
    inline uint32_t& mutable_feature_max_age_us() { return feature_max_age_us_.get(); }
    inline const uint32_t& get_feature_max_age_us() const { return feature_max_age_us_.get(); }
    inline uint32_t feature_max_age_us() const { return feature_max_age_us_.get(); }

    static constexpr char const* FEATURE_MAX_SKEW_US_NAME = "feature_max_skew_us";
    inline void clear_feature_max_skew_us() { feature_max_skew_us_.clear(); }
    inline void set_feature_max_skew_us(const uint32_t& value) { feature_max_skew_us_ = value; }
    inline void set_feature_max_skew_us(const uint32_t&& value) { feature_max_skew_us_ = value; }
    inline uint32_t& mutable_feature_max_skew_us() { return feature_max_skew_us_.get(); }
    inline const uint32_t& get_feature_max_skew_us() const { return feature_max_skew_us_.get(); }
    inline uint32_t feature_max_skew_us() const { return feature_max_skew_us_.get(); }

    static constexpr char const* FEATURE_COAST_ALLOWANCE_M_NAME = "feature_coast_allowance_m";
    inline void clear_feature_coast_allowance_m() { feature_coast_allowance_m_.clear(); }
    inline void set_feature_coast_allowance_m(const float& value) { feature_coast_allowance_m_ = value; }
    inline void set_feature_coast_allowance_m(const float&& value) { feature_coast_allowance_m_ = value; }
    inline float& mutable_feature_coast_allowance_m() { return feature_coast_allowance_m_.get(); }
    inline const float& get_feature_coast_allowance_m() const { return feature_coast_allowance_m_.get(); }
    inline float feature_coast_allowance_m() const { return feature_coast_allowance_m_.get(); }

    static constexpr char const* FEATURE_COMMAND_LATENCY_S_NAME = "feature_command_latency_s";
    inline void clear_feature_command_latency_s() { feature_command_latency_s_.clear(); }
    inline void set_feature_command_latency_s(const float& value) { feature_command_latency_s_ = value; }
    inline void set_feature_command_latency_s(const float&& value) { feature_command_latency_s_ = value; }
    inline float& mutable_feature_command_latency_s() { return feature_command_latency_s_.get(); }
    inline const float& get_feature_command_latency_s() const { return feature_command_latency_s_.get(); }
    inline float feature_command_latency_s() const { return feature_command_latency_s_.get(); }

    static constexpr char const* RHR_SPEED_M_S_NAME = "rhr_speed_m_s";
    inline void clear_rhr_speed_m_s() { rhr_speed_m_s_.clear(); }
    inline void set_rhr_speed_m_s(const float& value) { rhr_speed_m_s_ = value; }
    inline void set_rhr_speed_m_s(const float&& value) { rhr_speed_m_s_ = value; }
    inline float& mutable_rhr_speed_m_s() { return rhr_speed_m_s_.get(); }
    inline const float& get_rhr_speed_m_s() const { return rhr_speed_m_s_.get(); }
    inline float rhr_speed_m_s() const { return rhr_speed_m_s_.get(); }

    static constexpr char const* RHR_TURN_RAD_S_NAME = "rhr_turn_rad_s";
    inline void clear_rhr_turn_rad_s() { rhr_turn_rad_s_.clear(); }
    inline void set_rhr_turn_rad_s(const float& value) { rhr_turn_rad_s_ = value; }
    inline void set_rhr_turn_rad_s(const float&& value) { rhr_turn_rad_s_ = value; }
    inline float& mutable_rhr_turn_rad_s() { return rhr_turn_rad_s_.get(); }
    inline const float& get_rhr_turn_rad_s() const { return rhr_turn_rad_s_.get(); }
    inline float rhr_turn_rad_s() const { return rhr_turn_rad_s_.get(); }

    static constexpr char const* RHR_WALL_DISTANCE_M_NAME = "rhr_wall_distance_m";
    inline void clear_rhr_wall_distance_m() { rhr_wall_distance_m_.clear(); }
    inline void set_rhr_wall_distance_m(const float& value) { rhr_wall_distance_m_ = value; }
    inline void set_rhr_wall_distance_m(const float&& value) { rhr_wall_distance_m_ = value; }
    inline float& mutable_rhr_wall_distance_m() { return rhr_wall_distance_m_.get(); }
    inline const float& get_rhr_wall_distance_m() const { return rhr_wall_distance_m_.get(); }
    inline float rhr_wall_distance_m() const { return rhr_wall_distance_m_.get(); }

    static constexpr char const* RHR_FRONT_STOP_M_NAME = "rhr_front_stop_m";
    inline void clear_rhr_front_stop_m() { rhr_front_stop_m_.clear(); }
    inline void set_rhr_front_stop_m(const float& value) { rhr_front_stop_m_ = value; }
    inline void set_rhr_front_stop_m(const float&& value) { rhr_front_stop_m_ = value; }
    inline float& mutable_rhr_front_stop_m() { return rhr_front_stop_m_.get(); }
    inline const float& get_rhr_front_stop_m() const { return rhr_front_stop_m_.get(); }
    inline float rhr_front_stop_m() const { return rhr_front_stop_m_.get(); }

    static constexpr char const* RHR_SIDE_STOP_M_NAME = "rhr_side_stop_m";
    inline void clear_rhr_side_stop_m() { rhr_side_stop_m_.clear(); }
    inline void set_rhr_side_stop_m(const float& value) { rhr_side_stop_m_ = value; }
    inline void set_rhr_side_stop_m(const float&& value) { rhr_side_stop_m_ = value; }
    inline float& mutable_rhr_side_stop_m() { return rhr_side_stop_m_.get(); }
    inline const float& get_rhr_side_stop_m() const { return rhr_side_stop_m_.get(); }
    inline float rhr_side_stop_m() const { return rhr_side_stop_m_.get(); }

    static constexpr char const* RHR_OPENING_M_NAME = "rhr_opening_m";
    inline void clear_rhr_opening_m() { rhr_opening_m_.clear(); }
    inline void set_rhr_opening_m(const float& value) { rhr_opening_m_ = value; }
    inline void set_rhr_opening_m(const float&& value) { rhr_opening_m_ = value; }
    inline float& mutable_rhr_opening_m() { return rhr_opening_m_.get(); }
    inline const float& get_rhr_opening_m() const { return rhr_opening_m_.get(); }
    inline float rhr_opening_m() const { return rhr_opening_m_.get(); }

    static constexpr char const* RHR_WALL_GAIN_NAME = "rhr_wall_gain";
    inline void clear_rhr_wall_gain() { rhr_wall_gain_.clear(); }
    inline void set_rhr_wall_gain(const float& value) { rhr_wall_gain_ = value; }
    inline void set_rhr_wall_gain(const float&& value) { rhr_wall_gain_ = value; }
    inline float& mutable_rhr_wall_gain() { return rhr_wall_gain_.get(); }
    inline const float& get_rhr_wall_gain() const { return rhr_wall_gain_.get(); }
    inline float rhr_wall_gain() const { return rhr_wall_gain_.get(); }

    static constexpr char const* RHR_MAX_WALL_TURN_RAD_S_NAME = "rhr_max_wall_turn_rad_s";
    inline void clear_rhr_max_wall_turn_rad_s() { rhr_max_wall_turn_rad_s_.clear(); }
    inline void set_rhr_max_wall_turn_rad_s(const float& value) { rhr_max_wall_turn_rad_s_ = value; }
    inline void set_rhr_max_wall_turn_rad_s(const float&& value) { rhr_max_wall_turn_rad_s_ = value; }
    inline float& mutable_rhr_max_wall_turn_rad_s() { return rhr_max_wall_turn_rad_s_.get(); }
    inline const float& get_rhr_max_wall_turn_rad_s() const { return rhr_max_wall_turn_rad_s_.get(); }
    inline float rhr_max_wall_turn_rad_s() const { return rhr_max_wall_turn_rad_s_.get(); }

    static constexpr char const* RHR_RANGE_MAX_AGE_US_NAME = "rhr_range_max_age_us";
    inline void clear_rhr_range_max_age_us() { rhr_range_max_age_us_.clear(); }
    inline void set_rhr_range_max_age_us(const uint32_t& value) { rhr_range_max_age_us_ = value; }
    inline void set_rhr_range_max_age_us(const uint32_t&& value) { rhr_range_max_age_us_ = value; }
    inline uint32_t& mutable_rhr_range_max_age_us() { return rhr_range_max_age_us_.get(); }
    inline const uint32_t& get_rhr_range_max_age_us() const { return rhr_range_max_age_us_.get(); }
    inline uint32_t rhr_range_max_age_us() const { return rhr_range_max_age_us_.get(); }

    static constexpr char const* RHR_IMU_MAX_AGE_US_NAME = "rhr_imu_max_age_us";
    inline void clear_rhr_imu_max_age_us() { rhr_imu_max_age_us_.clear(); }
    inline void set_rhr_imu_max_age_us(const uint32_t& value) { rhr_imu_max_age_us_ = value; }
    inline void set_rhr_imu_max_age_us(const uint32_t&& value) { rhr_imu_max_age_us_ = value; }
    inline uint32_t& mutable_rhr_imu_max_age_us() { return rhr_imu_max_age_us_.get(); }
    inline const uint32_t& get_rhr_imu_max_age_us() const { return rhr_imu_max_age_us_.get(); }
    inline uint32_t rhr_imu_max_age_us() const { return rhr_imu_max_age_us_.get(); }

    static constexpr char const* RHR_TURN_TIMEOUT_US_NAME = "rhr_turn_timeout_us";
    inline void clear_rhr_turn_timeout_us() { rhr_turn_timeout_us_.clear(); }
    inline void set_rhr_turn_timeout_us(const uint32_t& value) { rhr_turn_timeout_us_ = value; }
    inline void set_rhr_turn_timeout_us(const uint32_t&& value) { rhr_turn_timeout_us_ = value; }
    inline uint32_t& mutable_rhr_turn_timeout_us() { return rhr_turn_timeout_us_.get(); }
    inline const uint32_t& get_rhr_turn_timeout_us() const { return rhr_turn_timeout_us_.get(); }
    inline uint32_t rhr_turn_timeout_us() const { return rhr_turn_timeout_us_.get(); }

    static constexpr char const* RHR_COAST_US_NAME = "rhr_coast_us";
    inline void clear_rhr_coast_us() { rhr_coast_us_.clear(); }
    inline void set_rhr_coast_us(const uint32_t& value) { rhr_coast_us_ = value; }
    inline void set_rhr_coast_us(const uint32_t&& value) { rhr_coast_us_ = value; }
    inline uint32_t& mutable_rhr_coast_us() { return rhr_coast_us_.get(); }
    inline const uint32_t& get_rhr_coast_us() const { return rhr_coast_us_.get(); }
    inline uint32_t rhr_coast_us() const { return rhr_coast_us_.get(); }

    static constexpr char const* RHR_CLEAR_US_NAME = "rhr_clear_us";
    inline void clear_rhr_clear_us() { rhr_clear_us_.clear(); }
    inline void set_rhr_clear_us(const uint32_t& value) { rhr_clear_us_ = value; }
    inline void set_rhr_clear_us(const uint32_t&& value) { rhr_clear_us_ = value; }
    inline uint32_t& mutable_rhr_clear_us() { return rhr_clear_us_.get(); }
    inline const uint32_t& get_rhr_clear_us() const { return rhr_clear_us_.get(); }
    inline uint32_t rhr_clear_us() const { return rhr_clear_us_.get(); }

    static constexpr char const* TAPE_BLACK_MAX_NAME = "tape_black_max";
    inline void clear_tape_black_max() { tape_black_max_.clear(); }
    inline void set_tape_black_max(const uint32_t& value) { tape_black_max_ = value; }
    inline void set_tape_black_max(const uint32_t&& value) { tape_black_max_ = value; }
    inline uint32_t& mutable_tape_black_max() { return tape_black_max_.get(); }
    inline const uint32_t& get_tape_black_max() const { return tape_black_max_.get(); }
    inline uint32_t tape_black_max() const { return tape_black_max_.get(); }

    static constexpr char const* TAPE_WHITE_MIN_NAME = "tape_white_min";
    inline void clear_tape_white_min() { tape_white_min_.clear(); }
    inline void set_tape_white_min(const uint32_t& value) { tape_white_min_ = value; }
    inline void set_tape_white_min(const uint32_t&& value) { tape_white_min_ = value; }
    inline uint32_t& mutable_tape_white_min() { return tape_white_min_.get(); }
    inline const uint32_t& get_tape_white_min() const { return tape_white_min_.get(); }
    inline uint32_t tape_white_min() const { return tape_white_min_.get(); }

    static constexpr char const* GYRO_ZERO_RPS_NAME = "gyro_zero_rps";
    inline void clear_gyro_zero_rps() { gyro_zero_rps_.clear(); }
    inline void set_gyro_zero_rps(const float& value) { gyro_zero_rps_ = value; }
    inline void set_gyro_zero_rps(const float&& value) { gyro_zero_rps_ = value; }
    inline float& mutable_gyro_zero_rps() { return gyro_zero_rps_.get(); }
    inline const float& get_gyro_zero_rps() const { return gyro_zero_rps_.get(); }
    inline float gyro_zero_rps() const { return gyro_zero_rps_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((false != legacy_color_0_enabled_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_0_enabled_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_ENABLED), buffer, false);
      }

      if((0.0f != legacy_color_0_red_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_0_red_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_RED), buffer, false);
      }

      if((0.0f != legacy_color_0_green_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_0_green_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_GREEN), buffer, false);
      }

      if((0.0f != legacy_color_0_blue_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_0_blue_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_BLUE), buffer, false);
      }

      if((0U != legacy_color_0_geometry_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_0_geometry_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_GEOMETRY), buffer, false);
      }

      if((0U != legacy_color_0_gx_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_0_gx_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_GX), buffer, false);
      }

      if((0U != legacy_color_0_gy_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_0_gy_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_GY), buffer, false);
      }

      if((false != legacy_color_1_enabled_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_1_enabled_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_ENABLED), buffer, false);
      }

      if((0.0f != legacy_color_1_red_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_1_red_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_RED), buffer, false);
      }

      if((0.0f != legacy_color_1_green_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_1_green_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_GREEN), buffer, false);
      }

      if((0.0f != legacy_color_1_blue_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_1_blue_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_BLUE), buffer, false);
      }

      if((0U != legacy_color_1_geometry_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_1_geometry_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_GEOMETRY), buffer, false);
      }

      if((0U != legacy_color_1_gx_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_1_gx_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_GX), buffer, false);
      }

      if((0U != legacy_color_1_gy_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_1_gy_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_GY), buffer, false);
      }

      if((false != legacy_color_2_enabled_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_2_enabled_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_ENABLED), buffer, false);
      }

      if((0.0f != legacy_color_2_red_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_2_red_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_RED), buffer, false);
      }

      if((0.0f != legacy_color_2_green_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_2_green_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_GREEN), buffer, false);
      }

      if((0.0f != legacy_color_2_blue_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_2_blue_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_BLUE), buffer, false);
      }

      if((0U != legacy_color_2_geometry_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_2_geometry_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_GEOMETRY), buffer, false);
      }

      if((0U != legacy_color_2_gx_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_2_gx_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_GX), buffer, false);
      }

      if((0U != legacy_color_2_gy_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_2_gy_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_GY), buffer, false);
      }

      if((false != legacy_color_3_enabled_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_3_enabled_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_ENABLED), buffer, false);
      }

      if((0.0f != legacy_color_3_red_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_3_red_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_RED), buffer, false);
      }

      if((0.0f != legacy_color_3_green_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_3_green_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_GREEN), buffer, false);
      }

      if((0.0f != legacy_color_3_blue_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_3_blue_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_BLUE), buffer, false);
      }

      if((0U != legacy_color_3_geometry_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_3_geometry_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_GEOMETRY), buffer, false);
      }

      if((0U != legacy_color_3_gx_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_3_gx_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_GX), buffer, false);
      }

      if((0U != legacy_color_3_gy_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = legacy_color_3_gy_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_GY), buffer, false);
      }

      if((0U != feature_variant_mask_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_variant_mask_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_VARIANT_MASK), buffer, false);
      }

      if((0.0f != feature_sigma_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_sigma_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_SIGMA_M), buffer, false);
      }

      if((0.0f != feature_max_score_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_max_score_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_MAX_SCORE), buffer, false);
      }

      if((0.0f != feature_min_margin_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_min_margin_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_MIN_MARGIN), buffer, false);
      }

      if((0.0f != feature_max_translation_m_s_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_max_translation_m_s_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_MAX_TRANSLATION_M_S), buffer, false);
      }

      if((0U != feature_confirmations_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_confirmations_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_CONFIRMATIONS), buffer, false);
      }

      if((0U != feature_contradiction_limit_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_contradiction_limit_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_CONTRADICTION_LIMIT), buffer, false);
      }

      if((0U != feature_max_age_us_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_max_age_us_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_MAX_AGE_US), buffer, false);
      }

      if((0U != feature_max_skew_us_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_max_skew_us_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_MAX_SKEW_US), buffer, false);
      }

      if((0.0f != feature_coast_allowance_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_coast_allowance_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_COAST_ALLOWANCE_M), buffer, false);
      }

      if((0.0f != feature_command_latency_s_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = feature_command_latency_s_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FEATURE_COMMAND_LATENCY_S), buffer, false);
      }

      if((0.0f != rhr_speed_m_s_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_speed_m_s_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_SPEED_M_S), buffer, false);
      }

      if((0.0f != rhr_turn_rad_s_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_turn_rad_s_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_TURN_RAD_S), buffer, false);
      }

      if((0.0f != rhr_wall_distance_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_wall_distance_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_WALL_DISTANCE_M), buffer, false);
      }

      if((0.0f != rhr_front_stop_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_front_stop_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_FRONT_STOP_M), buffer, false);
      }

      if((0.0f != rhr_side_stop_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_side_stop_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_SIDE_STOP_M), buffer, false);
      }

      if((0.0f != rhr_opening_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_opening_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_OPENING_M), buffer, false);
      }

      if((0.0f != rhr_wall_gain_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_wall_gain_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_WALL_GAIN), buffer, false);
      }

      if((0.0f != rhr_max_wall_turn_rad_s_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_max_wall_turn_rad_s_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_MAX_WALL_TURN_RAD_S), buffer, false);
      }

      if((0U != rhr_range_max_age_us_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_range_max_age_us_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_RANGE_MAX_AGE_US), buffer, false);
      }

      if((0U != rhr_imu_max_age_us_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_imu_max_age_us_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_IMU_MAX_AGE_US), buffer, false);
      }

      if((0U != rhr_turn_timeout_us_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_turn_timeout_us_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_TURN_TIMEOUT_US), buffer, false);
      }

      if((0U != rhr_coast_us_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_coast_us_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_COAST_US), buffer, false);
      }

      if((0U != rhr_clear_us_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = rhr_clear_us_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RHR_CLEAR_US), buffer, false);
      }

      if((0U != tape_black_max_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = tape_black_max_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TAPE_BLACK_MAX), buffer, false);
      }

      if((0U != tape_white_min_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = tape_white_min_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TAPE_WHITE_MIN), buffer, false);
      }

      if((0.0f != gyro_zero_rps_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = gyro_zero_rps_.serialize_with_id(static_cast<uint32_t>(FieldNumber::GYRO_ZERO_RPS), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::LEGACY_COLOR_0_ENABLED:
            return_value = legacy_color_0_enabled_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_0_RED:
            return_value = legacy_color_0_red_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_0_GREEN:
            return_value = legacy_color_0_green_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_0_BLUE:
            return_value = legacy_color_0_blue_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_0_GEOMETRY:
            return_value = legacy_color_0_geometry_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_0_GX:
            return_value = legacy_color_0_gx_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_0_GY:
            return_value = legacy_color_0_gy_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_1_ENABLED:
            return_value = legacy_color_1_enabled_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_1_RED:
            return_value = legacy_color_1_red_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_1_GREEN:
            return_value = legacy_color_1_green_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_1_BLUE:
            return_value = legacy_color_1_blue_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_1_GEOMETRY:
            return_value = legacy_color_1_geometry_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_1_GX:
            return_value = legacy_color_1_gx_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_1_GY:
            return_value = legacy_color_1_gy_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_2_ENABLED:
            return_value = legacy_color_2_enabled_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_2_RED:
            return_value = legacy_color_2_red_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_2_GREEN:
            return_value = legacy_color_2_green_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_2_BLUE:
            return_value = legacy_color_2_blue_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_2_GEOMETRY:
            return_value = legacy_color_2_geometry_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_2_GX:
            return_value = legacy_color_2_gx_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_2_GY:
            return_value = legacy_color_2_gy_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_3_ENABLED:
            return_value = legacy_color_3_enabled_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_3_RED:
            return_value = legacy_color_3_red_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_3_GREEN:
            return_value = legacy_color_3_green_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_3_BLUE:
            return_value = legacy_color_3_blue_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_3_GEOMETRY:
            return_value = legacy_color_3_geometry_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_3_GX:
            return_value = legacy_color_3_gx_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEGACY_COLOR_3_GY:
            return_value = legacy_color_3_gy_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_VARIANT_MASK:
            return_value = feature_variant_mask_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_SIGMA_M:
            return_value = feature_sigma_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_MAX_SCORE:
            return_value = feature_max_score_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_MIN_MARGIN:
            return_value = feature_min_margin_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_MAX_TRANSLATION_M_S:
            return_value = feature_max_translation_m_s_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_CONFIRMATIONS:
            return_value = feature_confirmations_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_CONTRADICTION_LIMIT:
            return_value = feature_contradiction_limit_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_MAX_AGE_US:
            return_value = feature_max_age_us_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_MAX_SKEW_US:
            return_value = feature_max_skew_us_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_COAST_ALLOWANCE_M:
            return_value = feature_coast_allowance_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FEATURE_COMMAND_LATENCY_S:
            return_value = feature_command_latency_s_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_SPEED_M_S:
            return_value = rhr_speed_m_s_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_TURN_RAD_S:
            return_value = rhr_turn_rad_s_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_WALL_DISTANCE_M:
            return_value = rhr_wall_distance_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_FRONT_STOP_M:
            return_value = rhr_front_stop_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_SIDE_STOP_M:
            return_value = rhr_side_stop_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_OPENING_M:
            return_value = rhr_opening_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_WALL_GAIN:
            return_value = rhr_wall_gain_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_MAX_WALL_TURN_RAD_S:
            return_value = rhr_max_wall_turn_rad_s_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_RANGE_MAX_AGE_US:
            return_value = rhr_range_max_age_us_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_IMU_MAX_AGE_US:
            return_value = rhr_imu_max_age_us_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_TURN_TIMEOUT_US:
            return_value = rhr_turn_timeout_us_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_COAST_US:
            return_value = rhr_coast_us_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RHR_CLEAR_US:
            return_value = rhr_clear_us_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::TAPE_BLACK_MAX:
            return_value = tape_black_max_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::TAPE_WHITE_MIN:
            return_value = tape_white_min_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::GYRO_ZERO_RPS:
            return_value = gyro_zero_rps_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_legacy_color_0_enabled();
      clear_legacy_color_0_red();
      clear_legacy_color_0_green();
      clear_legacy_color_0_blue();
      clear_legacy_color_0_geometry();
      clear_legacy_color_0_gx();
      clear_legacy_color_0_gy();
      clear_legacy_color_1_enabled();
      clear_legacy_color_1_red();
      clear_legacy_color_1_green();
      clear_legacy_color_1_blue();
      clear_legacy_color_1_geometry();
      clear_legacy_color_1_gx();
      clear_legacy_color_1_gy();
      clear_legacy_color_2_enabled();
      clear_legacy_color_2_red();
      clear_legacy_color_2_green();
      clear_legacy_color_2_blue();
      clear_legacy_color_2_geometry();
      clear_legacy_color_2_gx();
      clear_legacy_color_2_gy();
      clear_legacy_color_3_enabled();
      clear_legacy_color_3_red();
      clear_legacy_color_3_green();
      clear_legacy_color_3_blue();
      clear_legacy_color_3_geometry();
      clear_legacy_color_3_gx();
      clear_legacy_color_3_gy();
      clear_feature_variant_mask();
      clear_feature_sigma_m();
      clear_feature_max_score();
      clear_feature_min_margin();
      clear_feature_max_translation_m_s();
      clear_feature_confirmations();
      clear_feature_contradiction_limit();
      clear_feature_max_age_us();
      clear_feature_max_skew_us();
      clear_feature_coast_allowance_m();
      clear_feature_command_latency_s();
      clear_rhr_speed_m_s();
      clear_rhr_turn_rad_s();
      clear_rhr_wall_distance_m();
      clear_rhr_front_stop_m();
      clear_rhr_side_stop_m();
      clear_rhr_opening_m();
      clear_rhr_wall_gain();
      clear_rhr_max_wall_turn_rad_s();
      clear_rhr_range_max_age_us();
      clear_rhr_imu_max_age_us();
      clear_rhr_turn_timeout_us();
      clear_rhr_coast_us();
      clear_rhr_clear_us();
      clear_tape_black_max();
      clear_tape_white_min();
      clear_gyro_zero_rps();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME 

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::LEGACY_COLOR_0_ENABLED:
          name = LEGACY_COLOR_0_ENABLED_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_0_RED:
          name = LEGACY_COLOR_0_RED_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_0_GREEN:
          name = LEGACY_COLOR_0_GREEN_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_0_BLUE:
          name = LEGACY_COLOR_0_BLUE_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_0_GEOMETRY:
          name = LEGACY_COLOR_0_GEOMETRY_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_0_GX:
          name = LEGACY_COLOR_0_GX_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_0_GY:
          name = LEGACY_COLOR_0_GY_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_1_ENABLED:
          name = LEGACY_COLOR_1_ENABLED_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_1_RED:
          name = LEGACY_COLOR_1_RED_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_1_GREEN:
          name = LEGACY_COLOR_1_GREEN_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_1_BLUE:
          name = LEGACY_COLOR_1_BLUE_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_1_GEOMETRY:
          name = LEGACY_COLOR_1_GEOMETRY_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_1_GX:
          name = LEGACY_COLOR_1_GX_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_1_GY:
          name = LEGACY_COLOR_1_GY_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_2_ENABLED:
          name = LEGACY_COLOR_2_ENABLED_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_2_RED:
          name = LEGACY_COLOR_2_RED_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_2_GREEN:
          name = LEGACY_COLOR_2_GREEN_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_2_BLUE:
          name = LEGACY_COLOR_2_BLUE_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_2_GEOMETRY:
          name = LEGACY_COLOR_2_GEOMETRY_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_2_GX:
          name = LEGACY_COLOR_2_GX_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_2_GY:
          name = LEGACY_COLOR_2_GY_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_3_ENABLED:
          name = LEGACY_COLOR_3_ENABLED_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_3_RED:
          name = LEGACY_COLOR_3_RED_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_3_GREEN:
          name = LEGACY_COLOR_3_GREEN_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_3_BLUE:
          name = LEGACY_COLOR_3_BLUE_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_3_GEOMETRY:
          name = LEGACY_COLOR_3_GEOMETRY_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_3_GX:
          name = LEGACY_COLOR_3_GX_NAME;
          break;
        case FieldNumber::LEGACY_COLOR_3_GY:
          name = LEGACY_COLOR_3_GY_NAME;
          break;
        case FieldNumber::FEATURE_VARIANT_MASK:
          name = FEATURE_VARIANT_MASK_NAME;
          break;
        case FieldNumber::FEATURE_SIGMA_M:
          name = FEATURE_SIGMA_M_NAME;
          break;
        case FieldNumber::FEATURE_MAX_SCORE:
          name = FEATURE_MAX_SCORE_NAME;
          break;
        case FieldNumber::FEATURE_MIN_MARGIN:
          name = FEATURE_MIN_MARGIN_NAME;
          break;
        case FieldNumber::FEATURE_MAX_TRANSLATION_M_S:
          name = FEATURE_MAX_TRANSLATION_M_S_NAME;
          break;
        case FieldNumber::FEATURE_CONFIRMATIONS:
          name = FEATURE_CONFIRMATIONS_NAME;
          break;
        case FieldNumber::FEATURE_CONTRADICTION_LIMIT:
          name = FEATURE_CONTRADICTION_LIMIT_NAME;
          break;
        case FieldNumber::FEATURE_MAX_AGE_US:
          name = FEATURE_MAX_AGE_US_NAME;
          break;
        case FieldNumber::FEATURE_MAX_SKEW_US:
          name = FEATURE_MAX_SKEW_US_NAME;
          break;
        case FieldNumber::FEATURE_COAST_ALLOWANCE_M:
          name = FEATURE_COAST_ALLOWANCE_M_NAME;
          break;
        case FieldNumber::FEATURE_COMMAND_LATENCY_S:
          name = FEATURE_COMMAND_LATENCY_S_NAME;
          break;
        case FieldNumber::RHR_SPEED_M_S:
          name = RHR_SPEED_M_S_NAME;
          break;
        case FieldNumber::RHR_TURN_RAD_S:
          name = RHR_TURN_RAD_S_NAME;
          break;
        case FieldNumber::RHR_WALL_DISTANCE_M:
          name = RHR_WALL_DISTANCE_M_NAME;
          break;
        case FieldNumber::RHR_FRONT_STOP_M:
          name = RHR_FRONT_STOP_M_NAME;
          break;
        case FieldNumber::RHR_SIDE_STOP_M:
          name = RHR_SIDE_STOP_M_NAME;
          break;
        case FieldNumber::RHR_OPENING_M:
          name = RHR_OPENING_M_NAME;
          break;
        case FieldNumber::RHR_WALL_GAIN:
          name = RHR_WALL_GAIN_NAME;
          break;
        case FieldNumber::RHR_MAX_WALL_TURN_RAD_S:
          name = RHR_MAX_WALL_TURN_RAD_S_NAME;
          break;
        case FieldNumber::RHR_RANGE_MAX_AGE_US:
          name = RHR_RANGE_MAX_AGE_US_NAME;
          break;
        case FieldNumber::RHR_IMU_MAX_AGE_US:
          name = RHR_IMU_MAX_AGE_US_NAME;
          break;
        case FieldNumber::RHR_TURN_TIMEOUT_US:
          name = RHR_TURN_TIMEOUT_US_NAME;
          break;
        case FieldNumber::RHR_COAST_US:
          name = RHR_COAST_US_NAME;
          break;
        case FieldNumber::RHR_CLEAR_US:
          name = RHR_CLEAR_US_NAME;
          break;
        case FieldNumber::TAPE_BLACK_MAX:
          name = TAPE_BLACK_MAX_NAME;
          break;
        case FieldNumber::TAPE_WHITE_MIN:
          name = TAPE_WHITE_MIN_NAME;
          break;
        case FieldNumber::GYRO_ZERO_RPS:
          name = GYRO_ZERO_RPS_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

    //! Calculate the maximum number of bytes required to serialize this type of message.
    /*!
      This function does not take into account a possible id of this message object is self.
      \return The number of bytes required at most to serialize this message type.
    */
    static constexpr uint32_t max_serialized_size()
    {
      using namespace ::EmbeddedProto;
      return
          EmbeddedProto::boolean::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_ENABLED))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_RED))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_GREEN))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_BLUE))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_GEOMETRY))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_GX))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_0_GY))
          + EmbeddedProto::boolean::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_ENABLED))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_RED))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_GREEN))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_BLUE))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_GEOMETRY))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_GX))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_1_GY))
          + EmbeddedProto::boolean::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_ENABLED))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_RED))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_GREEN))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_BLUE))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_GEOMETRY))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_GX))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_2_GY))
          + EmbeddedProto::boolean::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_ENABLED))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_RED))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_GREEN))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_BLUE))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_GEOMETRY))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_GX))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEGACY_COLOR_3_GY))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_VARIANT_MASK))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_SIGMA_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_MAX_SCORE))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_MIN_MARGIN))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_MAX_TRANSLATION_M_S))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_CONFIRMATIONS))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_CONTRADICTION_LIMIT))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_MAX_AGE_US))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_MAX_SKEW_US))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_COAST_ALLOWANCE_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::FEATURE_COMMAND_LATENCY_S))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_SPEED_M_S))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_TURN_RAD_S))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_WALL_DISTANCE_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_FRONT_STOP_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_SIDE_STOP_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_OPENING_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_WALL_GAIN))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_MAX_WALL_TURN_RAD_S))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_RANGE_MAX_AGE_US))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_IMU_MAX_AGE_US))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_TURN_TIMEOUT_US))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_COAST_US))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::RHR_CLEAR_US))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::TAPE_BLACK_MAX))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::TAPE_WHITE_MIN))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::GYRO_ZERO_RPS))
        ;
    }

    //! Calculate the maximum number of bytes required to serialize this message including the field id number and tag.
    /*!
      \return The number of bytes required at most to serialize this message type.
    */
    static constexpr uint32_t max_serialized_size(const uint32_t field_number)
    {
      using namespace ::EmbeddedProto;
      return WireFormatter::VarintSize(WireFormatter::MakeTag(field_number, WireFormatter::WireType::LENGTH_DELIMITED)) // Size of the tag
        + WireFormatter::VarintSize(max_serialized_size()) // Length delimted value
        + max_serialized_size(); // Length of the serialized content
    }

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          const int32_t actual_chars_used = EmbeddedProto::min(n_chars_used, left_chars.size);
          left_chars.data += actual_chars_used;
          left_chars.size -= actual_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }
      
      if(0 < n_chars_used)
      {
        const int32_t actual_chars_used = EmbeddedProto::min(n_chars_used, left_chars.size);
        left_chars.data += actual_chars_used;
        left_chars.size -= actual_chars_used;
      }

      left_chars = legacy_color_0_enabled_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_0_ENABLED_NAME, true);
      left_chars = legacy_color_0_red_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_0_RED_NAME, false);
      left_chars = legacy_color_0_green_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_0_GREEN_NAME, false);
      left_chars = legacy_color_0_blue_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_0_BLUE_NAME, false);
      left_chars = legacy_color_0_geometry_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_0_GEOMETRY_NAME, false);
      left_chars = legacy_color_0_gx_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_0_GX_NAME, false);
      left_chars = legacy_color_0_gy_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_0_GY_NAME, false);
      left_chars = legacy_color_1_enabled_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_1_ENABLED_NAME, false);
      left_chars = legacy_color_1_red_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_1_RED_NAME, false);
      left_chars = legacy_color_1_green_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_1_GREEN_NAME, false);
      left_chars = legacy_color_1_blue_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_1_BLUE_NAME, false);
      left_chars = legacy_color_1_geometry_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_1_GEOMETRY_NAME, false);
      left_chars = legacy_color_1_gx_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_1_GX_NAME, false);
      left_chars = legacy_color_1_gy_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_1_GY_NAME, false);
      left_chars = legacy_color_2_enabled_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_2_ENABLED_NAME, false);
      left_chars = legacy_color_2_red_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_2_RED_NAME, false);
      left_chars = legacy_color_2_green_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_2_GREEN_NAME, false);
      left_chars = legacy_color_2_blue_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_2_BLUE_NAME, false);
      left_chars = legacy_color_2_geometry_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_2_GEOMETRY_NAME, false);
      left_chars = legacy_color_2_gx_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_2_GX_NAME, false);
      left_chars = legacy_color_2_gy_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_2_GY_NAME, false);
      left_chars = legacy_color_3_enabled_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_3_ENABLED_NAME, false);
      left_chars = legacy_color_3_red_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_3_RED_NAME, false);
      left_chars = legacy_color_3_green_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_3_GREEN_NAME, false);
      left_chars = legacy_color_3_blue_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_3_BLUE_NAME, false);
      left_chars = legacy_color_3_geometry_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_3_GEOMETRY_NAME, false);
      left_chars = legacy_color_3_gx_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_3_GX_NAME, false);
      left_chars = legacy_color_3_gy_.to_string(left_chars, indent_level + 2, LEGACY_COLOR_3_GY_NAME, false);
      left_chars = feature_variant_mask_.to_string(left_chars, indent_level + 2, FEATURE_VARIANT_MASK_NAME, false);
      left_chars = feature_sigma_m_.to_string(left_chars, indent_level + 2, FEATURE_SIGMA_M_NAME, false);
      left_chars = feature_max_score_.to_string(left_chars, indent_level + 2, FEATURE_MAX_SCORE_NAME, false);
      left_chars = feature_min_margin_.to_string(left_chars, indent_level + 2, FEATURE_MIN_MARGIN_NAME, false);
      left_chars = feature_max_translation_m_s_.to_string(left_chars, indent_level + 2, FEATURE_MAX_TRANSLATION_M_S_NAME, false);
      left_chars = feature_confirmations_.to_string(left_chars, indent_level + 2, FEATURE_CONFIRMATIONS_NAME, false);
      left_chars = feature_contradiction_limit_.to_string(left_chars, indent_level + 2, FEATURE_CONTRADICTION_LIMIT_NAME, false);
      left_chars = feature_max_age_us_.to_string(left_chars, indent_level + 2, FEATURE_MAX_AGE_US_NAME, false);
      left_chars = feature_max_skew_us_.to_string(left_chars, indent_level + 2, FEATURE_MAX_SKEW_US_NAME, false);
      left_chars = feature_coast_allowance_m_.to_string(left_chars, indent_level + 2, FEATURE_COAST_ALLOWANCE_M_NAME, false);
      left_chars = feature_command_latency_s_.to_string(left_chars, indent_level + 2, FEATURE_COMMAND_LATENCY_S_NAME, false);
      left_chars = rhr_speed_m_s_.to_string(left_chars, indent_level + 2, RHR_SPEED_M_S_NAME, false);
      left_chars = rhr_turn_rad_s_.to_string(left_chars, indent_level + 2, RHR_TURN_RAD_S_NAME, false);
      left_chars = rhr_wall_distance_m_.to_string(left_chars, indent_level + 2, RHR_WALL_DISTANCE_M_NAME, false);
      left_chars = rhr_front_stop_m_.to_string(left_chars, indent_level + 2, RHR_FRONT_STOP_M_NAME, false);
      left_chars = rhr_side_stop_m_.to_string(left_chars, indent_level + 2, RHR_SIDE_STOP_M_NAME, false);
      left_chars = rhr_opening_m_.to_string(left_chars, indent_level + 2, RHR_OPENING_M_NAME, false);
      left_chars = rhr_wall_gain_.to_string(left_chars, indent_level + 2, RHR_WALL_GAIN_NAME, false);
      left_chars = rhr_max_wall_turn_rad_s_.to_string(left_chars, indent_level + 2, RHR_MAX_WALL_TURN_RAD_S_NAME, false);
      left_chars = rhr_range_max_age_us_.to_string(left_chars, indent_level + 2, RHR_RANGE_MAX_AGE_US_NAME, false);
      left_chars = rhr_imu_max_age_us_.to_string(left_chars, indent_level + 2, RHR_IMU_MAX_AGE_US_NAME, false);
      left_chars = rhr_turn_timeout_us_.to_string(left_chars, indent_level + 2, RHR_TURN_TIMEOUT_US_NAME, false);
      left_chars = rhr_coast_us_.to_string(left_chars, indent_level + 2, RHR_COAST_US_NAME, false);
      left_chars = rhr_clear_us_.to_string(left_chars, indent_level + 2, RHR_CLEAR_US_NAME, false);
      left_chars = tape_black_max_.to_string(left_chars, indent_level + 2, TAPE_BLACK_MAX_NAME, false);
      left_chars = tape_white_min_.to_string(left_chars, indent_level + 2, TAPE_WHITE_MIN_NAME, false);
      left_chars = gyro_zero_rps_.to_string(left_chars, indent_level + 2, GYRO_ZERO_RPS_NAME, false);
  
      if( 0 == indent_level) 
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else 
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        const int32_t actual_chars_used = EmbeddedProto::min(n_chars_used, left_chars.size);
        left_chars.data += actual_chars_used;
        left_chars.size -= actual_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::boolean legacy_color_0_enabled_ = false;
      EmbeddedProto::floatfixed legacy_color_0_red_ = 0.0f;
      EmbeddedProto::floatfixed legacy_color_0_green_ = 0.0f;
      EmbeddedProto::floatfixed legacy_color_0_blue_ = 0.0f;
      EmbeddedProto::uint32 legacy_color_0_geometry_ = 0U;
      EmbeddedProto::uint32 legacy_color_0_gx_ = 0U;
      EmbeddedProto::uint32 legacy_color_0_gy_ = 0U;
      EmbeddedProto::boolean legacy_color_1_enabled_ = false;
      EmbeddedProto::floatfixed legacy_color_1_red_ = 0.0f;
      EmbeddedProto::floatfixed legacy_color_1_green_ = 0.0f;
      EmbeddedProto::floatfixed legacy_color_1_blue_ = 0.0f;
      EmbeddedProto::uint32 legacy_color_1_geometry_ = 0U;
      EmbeddedProto::uint32 legacy_color_1_gx_ = 0U;
      EmbeddedProto::uint32 legacy_color_1_gy_ = 0U;
      EmbeddedProto::boolean legacy_color_2_enabled_ = false;
      EmbeddedProto::floatfixed legacy_color_2_red_ = 0.0f;
      EmbeddedProto::floatfixed legacy_color_2_green_ = 0.0f;
      EmbeddedProto::floatfixed legacy_color_2_blue_ = 0.0f;
      EmbeddedProto::uint32 legacy_color_2_geometry_ = 0U;
      EmbeddedProto::uint32 legacy_color_2_gx_ = 0U;
      EmbeddedProto::uint32 legacy_color_2_gy_ = 0U;
      EmbeddedProto::boolean legacy_color_3_enabled_ = false;
      EmbeddedProto::floatfixed legacy_color_3_red_ = 0.0f;
      EmbeddedProto::floatfixed legacy_color_3_green_ = 0.0f;
      EmbeddedProto::floatfixed legacy_color_3_blue_ = 0.0f;
      EmbeddedProto::uint32 legacy_color_3_geometry_ = 0U;
      EmbeddedProto::uint32 legacy_color_3_gx_ = 0U;
      EmbeddedProto::uint32 legacy_color_3_gy_ = 0U;
      EmbeddedProto::uint32 feature_variant_mask_ = 0U;
      EmbeddedProto::floatfixed feature_sigma_m_ = 0.0f;
      EmbeddedProto::floatfixed feature_max_score_ = 0.0f;
      EmbeddedProto::floatfixed feature_min_margin_ = 0.0f;
      EmbeddedProto::floatfixed feature_max_translation_m_s_ = 0.0f;
      EmbeddedProto::uint32 feature_confirmations_ = 0U;
      EmbeddedProto::uint32 feature_contradiction_limit_ = 0U;
      EmbeddedProto::uint32 feature_max_age_us_ = 0U;
      EmbeddedProto::uint32 feature_max_skew_us_ = 0U;
      EmbeddedProto::floatfixed feature_coast_allowance_m_ = 0.0f;
      EmbeddedProto::floatfixed feature_command_latency_s_ = 0.0f;
      EmbeddedProto::floatfixed rhr_speed_m_s_ = 0.0f;
      EmbeddedProto::floatfixed rhr_turn_rad_s_ = 0.0f;
      EmbeddedProto::floatfixed rhr_wall_distance_m_ = 0.0f;
      EmbeddedProto::floatfixed rhr_front_stop_m_ = 0.0f;
      EmbeddedProto::floatfixed rhr_side_stop_m_ = 0.0f;
      EmbeddedProto::floatfixed rhr_opening_m_ = 0.0f;
      EmbeddedProto::floatfixed rhr_wall_gain_ = 0.0f;
      EmbeddedProto::floatfixed rhr_max_wall_turn_rad_s_ = 0.0f;
      EmbeddedProto::uint32 rhr_range_max_age_us_ = 0U;
      EmbeddedProto::uint32 rhr_imu_max_age_us_ = 0U;
      EmbeddedProto::uint32 rhr_turn_timeout_us_ = 0U;
      EmbeddedProto::uint32 rhr_coast_us_ = 0U;
      EmbeddedProto::uint32 rhr_clear_us_ = 0U;
      EmbeddedProto::uint32 tape_black_max_ = 0U;
      EmbeddedProto::uint32 tape_white_min_ = 0U;
      EmbeddedProto::floatfixed gyro_zero_rps_ = 0.0f;

};

} // End of namespace v1
} // End of namespace remote
} // End of namespace axi
#endif // FLASH_SETTINGS_H