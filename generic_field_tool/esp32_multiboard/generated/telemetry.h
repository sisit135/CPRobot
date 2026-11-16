/*
 *  This file is generated with Embedded Proto, PLEASE DO NOT EDIT!
 *  source: telemetry.proto
 */

// This file is generated. Please do not edit!
#ifndef TELEMETRY_H
#define TELEMETRY_H

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
namespace telemetry {
namespace v1 {

class Control final: public ::EmbeddedProto::MessageInterface
{
  public:
    Control() = default;
    Control(const Control& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
    }

    Control(const Control&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
    }

    ~Control() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      OPERATION = 2
    };

    Control& operator=(const Control& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      return *this;
    }

    Control& operator=(const Control&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      return *this;
    }

    static constexpr char const* REQUEST_ID_NAME = "request_id";
    inline void clear_request_id() { request_id_.clear(); }
    inline void set_request_id(const uint32_t& value) { request_id_ = value; }
    inline void set_request_id(const uint32_t&& value) { request_id_ = value; }
    inline uint32_t& mutable_request_id() { return request_id_.get(); }
    inline const uint32_t& get_request_id() const { return request_id_.get(); }
    inline uint32_t request_id() const { return request_id_.get(); }

    static constexpr char const* OPERATION_NAME = "operation";
    inline void clear_operation() { operation_.clear(); }
    inline void set_operation(const uint32_t& value) { operation_ = value; }
    inline void set_operation(const uint32_t&& value) { operation_ = value; }
    inline uint32_t& mutable_operation() { return operation_.get(); }
    inline const uint32_t& get_operation() const { return operation_.get(); }
    inline uint32_t operation() const { return operation_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if((0U != operation_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = operation_.serialize_with_id(static_cast<uint32_t>(FieldNumber::OPERATION), buffer, false);
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
          case FieldNumber::REQUEST_ID:
            return_value = request_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::OPERATION:
            return_value = operation_.deserialize_check_type(buffer, wire_type);
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
      clear_request_id();
      clear_operation();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME 

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::REQUEST_ID:
          name = REQUEST_ID_NAME;
          break;
        case FieldNumber::OPERATION:
          name = OPERATION_NAME;
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
          EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::REQUEST_ID))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::OPERATION))
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

      left_chars = request_id_.to_string(left_chars, indent_level + 2, REQUEST_ID_NAME, true);
      left_chars = operation_.to_string(left_chars, indent_level + 2, OPERATION_NAME, false);
  
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


      EmbeddedProto::uint32 request_id_ = 0U;
      EmbeddedProto::uint32 operation_ = 0U;

};

class ControlAck final: public ::EmbeddedProto::MessageInterface
{
  public:
    ControlAck() = default;
    ControlAck(const ControlAck& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      set_status(rhs.get_status());
    }

    ControlAck(const ControlAck&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      set_status(rhs.get_status());
    }

    ~ControlAck() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      OPERATION = 2,
      STATUS = 3
    };

    ControlAck& operator=(const ControlAck& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      set_status(rhs.get_status());
      return *this;
    }

    ControlAck& operator=(const ControlAck&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      set_status(rhs.get_status());
      return *this;
    }

    static constexpr char const* REQUEST_ID_NAME = "request_id";
    inline void clear_request_id() { request_id_.clear(); }
    inline void set_request_id(const uint32_t& value) { request_id_ = value; }
    inline void set_request_id(const uint32_t&& value) { request_id_ = value; }
    inline uint32_t& mutable_request_id() { return request_id_.get(); }
    inline const uint32_t& get_request_id() const { return request_id_.get(); }
    inline uint32_t request_id() const { return request_id_.get(); }

    static constexpr char const* OPERATION_NAME = "operation";
    inline void clear_operation() { operation_.clear(); }
    inline void set_operation(const uint32_t& value) { operation_ = value; }
    inline void set_operation(const uint32_t&& value) { operation_ = value; }
    inline uint32_t& mutable_operation() { return operation_.get(); }
    inline const uint32_t& get_operation() const { return operation_.get(); }
    inline uint32_t operation() const { return operation_.get(); }

    static constexpr char const* STATUS_NAME = "status";
    inline void clear_status() { status_.clear(); }
    inline void set_status(const uint32_t& value) { status_ = value; }
    inline void set_status(const uint32_t&& value) { status_ = value; }
    inline uint32_t& mutable_status() { return status_.get(); }
    inline const uint32_t& get_status() const { return status_.get(); }
    inline uint32_t status() const { return status_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if((0U != operation_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = operation_.serialize_with_id(static_cast<uint32_t>(FieldNumber::OPERATION), buffer, false);
      }

      if((0U != status_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = status_.serialize_with_id(static_cast<uint32_t>(FieldNumber::STATUS), buffer, false);
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
          case FieldNumber::REQUEST_ID:
            return_value = request_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::OPERATION:
            return_value = operation_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::STATUS:
            return_value = status_.deserialize_check_type(buffer, wire_type);
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
      clear_request_id();
      clear_operation();
      clear_status();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME 

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::REQUEST_ID:
          name = REQUEST_ID_NAME;
          break;
        case FieldNumber::OPERATION:
          name = OPERATION_NAME;
          break;
        case FieldNumber::STATUS:
          name = STATUS_NAME;
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
          EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::REQUEST_ID))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::OPERATION))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::STATUS))
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

      left_chars = request_id_.to_string(left_chars, indent_level + 2, REQUEST_ID_NAME, true);
      left_chars = operation_.to_string(left_chars, indent_level + 2, OPERATION_NAME, false);
      left_chars = status_.to_string(left_chars, indent_level + 2, STATUS_NAME, false);
  
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


      EmbeddedProto::uint32 request_id_ = 0U;
      EmbeddedProto::uint32 operation_ = 0U;
      EmbeddedProto::uint32 status_ = 0U;

};

class EkfLog final: public ::EmbeddedProto::MessageInterface
{
  public:
    EkfLog() = default;
    EkfLog(const EkfLog& rhs )
    {
      set_boot_id(rhs.get_boot_id());
      set_monotonic_us(rhs.get_monotonic_us());
      set_sequence(rhs.get_sequence());
      set_x_m(rhs.get_x_m());
      set_y_m(rhs.get_y_m());
      set_yaw_rad(rhs.get_yaw_rad());
      set_gyro_bias_rps(rhs.get_gyro_bias_rps());
      set_sigma_x_m(rhs.get_sigma_x_m());
      set_sigma_y_m(rhs.get_sigma_y_m());
      set_sigma_yaw_rad(rhs.get_sigma_yaw_rad());
      set_grid_x(rhs.get_grid_x());
      set_grid_y(rhs.get_grid_y());
      set_tracking(rhs.get_tracking());
      set_grid_confident(rhs.get_grid_confident());
      set_queue_dropped(rhs.get_queue_dropped());
    }

    EkfLog(const EkfLog&& rhs ) noexcept
    {
      set_boot_id(rhs.get_boot_id());
      set_monotonic_us(rhs.get_monotonic_us());
      set_sequence(rhs.get_sequence());
      set_x_m(rhs.get_x_m());
      set_y_m(rhs.get_y_m());
      set_yaw_rad(rhs.get_yaw_rad());
      set_gyro_bias_rps(rhs.get_gyro_bias_rps());
      set_sigma_x_m(rhs.get_sigma_x_m());
      set_sigma_y_m(rhs.get_sigma_y_m());
      set_sigma_yaw_rad(rhs.get_sigma_yaw_rad());
      set_grid_x(rhs.get_grid_x());
      set_grid_y(rhs.get_grid_y());
      set_tracking(rhs.get_tracking());
      set_grid_confident(rhs.get_grid_confident());
      set_queue_dropped(rhs.get_queue_dropped());
    }

    ~EkfLog() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      BOOT_ID = 1,
      MONOTONIC_US = 2,
      SEQUENCE = 3,
      X_M = 4,
      Y_M = 5,
      YAW_RAD = 6,
      GYRO_BIAS_RPS = 7,
      SIGMA_X_M = 8,
      SIGMA_Y_M = 9,
      SIGMA_YAW_RAD = 10,
      GRID_X = 11,
      GRID_Y = 12,
      TRACKING = 13,
      GRID_CONFIDENT = 14,
      QUEUE_DROPPED = 15
    };

    EkfLog& operator=(const EkfLog& rhs)
    {
      set_boot_id(rhs.get_boot_id());
      set_monotonic_us(rhs.get_monotonic_us());
      set_sequence(rhs.get_sequence());
      set_x_m(rhs.get_x_m());
      set_y_m(rhs.get_y_m());
      set_yaw_rad(rhs.get_yaw_rad());
      set_gyro_bias_rps(rhs.get_gyro_bias_rps());
      set_sigma_x_m(rhs.get_sigma_x_m());
      set_sigma_y_m(rhs.get_sigma_y_m());
      set_sigma_yaw_rad(rhs.get_sigma_yaw_rad());
      set_grid_x(rhs.get_grid_x());
      set_grid_y(rhs.get_grid_y());
      set_tracking(rhs.get_tracking());
      set_grid_confident(rhs.get_grid_confident());
      set_queue_dropped(rhs.get_queue_dropped());
      return *this;
    }

    EkfLog& operator=(const EkfLog&& rhs) noexcept
    {
      set_boot_id(rhs.get_boot_id());
      set_monotonic_us(rhs.get_monotonic_us());
      set_sequence(rhs.get_sequence());
      set_x_m(rhs.get_x_m());
      set_y_m(rhs.get_y_m());
      set_yaw_rad(rhs.get_yaw_rad());
      set_gyro_bias_rps(rhs.get_gyro_bias_rps());
      set_sigma_x_m(rhs.get_sigma_x_m());
      set_sigma_y_m(rhs.get_sigma_y_m());
      set_sigma_yaw_rad(rhs.get_sigma_yaw_rad());
      set_grid_x(rhs.get_grid_x());
      set_grid_y(rhs.get_grid_y());
      set_tracking(rhs.get_tracking());
      set_grid_confident(rhs.get_grid_confident());
      set_queue_dropped(rhs.get_queue_dropped());
      return *this;
    }

    static constexpr char const* BOOT_ID_NAME = "boot_id";
    inline void clear_boot_id() { boot_id_.clear(); }
    inline void set_boot_id(const uint32_t& value) { boot_id_ = value; }
    inline void set_boot_id(const uint32_t&& value) { boot_id_ = value; }
    inline uint32_t& mutable_boot_id() { return boot_id_.get(); }
    inline const uint32_t& get_boot_id() const { return boot_id_.get(); }
    inline uint32_t boot_id() const { return boot_id_.get(); }

    static constexpr char const* MONOTONIC_US_NAME = "monotonic_us";
    inline void clear_monotonic_us() { monotonic_us_.clear(); }
    inline void set_monotonic_us(const uint64_t& value) { monotonic_us_ = value; }
    inline void set_monotonic_us(const uint64_t&& value) { monotonic_us_ = value; }
    inline uint64_t& mutable_monotonic_us() { return monotonic_us_.get(); }
    inline const uint64_t& get_monotonic_us() const { return monotonic_us_.get(); }
    inline uint64_t monotonic_us() const { return monotonic_us_.get(); }

    static constexpr char const* SEQUENCE_NAME = "sequence";
    inline void clear_sequence() { sequence_.clear(); }
    inline void set_sequence(const uint32_t& value) { sequence_ = value; }
    inline void set_sequence(const uint32_t&& value) { sequence_ = value; }
    inline uint32_t& mutable_sequence() { return sequence_.get(); }
    inline const uint32_t& get_sequence() const { return sequence_.get(); }
    inline uint32_t sequence() const { return sequence_.get(); }

    static constexpr char const* X_M_NAME = "x_m";
    inline void clear_x_m() { x_m_.clear(); }
    inline void set_x_m(const float& value) { x_m_ = value; }
    inline void set_x_m(const float&& value) { x_m_ = value; }
    inline float& mutable_x_m() { return x_m_.get(); }
    inline const float& get_x_m() const { return x_m_.get(); }
    inline float x_m() const { return x_m_.get(); }

    static constexpr char const* Y_M_NAME = "y_m";
    inline void clear_y_m() { y_m_.clear(); }
    inline void set_y_m(const float& value) { y_m_ = value; }
    inline void set_y_m(const float&& value) { y_m_ = value; }
    inline float& mutable_y_m() { return y_m_.get(); }
    inline const float& get_y_m() const { return y_m_.get(); }
    inline float y_m() const { return y_m_.get(); }

    static constexpr char const* YAW_RAD_NAME = "yaw_rad";
    inline void clear_yaw_rad() { yaw_rad_.clear(); }
    inline void set_yaw_rad(const float& value) { yaw_rad_ = value; }
    inline void set_yaw_rad(const float&& value) { yaw_rad_ = value; }
    inline float& mutable_yaw_rad() { return yaw_rad_.get(); }
    inline const float& get_yaw_rad() const { return yaw_rad_.get(); }
    inline float yaw_rad() const { return yaw_rad_.get(); }

    static constexpr char const* GYRO_BIAS_RPS_NAME = "gyro_bias_rps";
    inline void clear_gyro_bias_rps() { gyro_bias_rps_.clear(); }
    inline void set_gyro_bias_rps(const float& value) { gyro_bias_rps_ = value; }
    inline void set_gyro_bias_rps(const float&& value) { gyro_bias_rps_ = value; }
    inline float& mutable_gyro_bias_rps() { return gyro_bias_rps_.get(); }
    inline const float& get_gyro_bias_rps() const { return gyro_bias_rps_.get(); }
    inline float gyro_bias_rps() const { return gyro_bias_rps_.get(); }

    static constexpr char const* SIGMA_X_M_NAME = "sigma_x_m";
    inline void clear_sigma_x_m() { sigma_x_m_.clear(); }
    inline void set_sigma_x_m(const float& value) { sigma_x_m_ = value; }
    inline void set_sigma_x_m(const float&& value) { sigma_x_m_ = value; }
    inline float& mutable_sigma_x_m() { return sigma_x_m_.get(); }
    inline const float& get_sigma_x_m() const { return sigma_x_m_.get(); }
    inline float sigma_x_m() const { return sigma_x_m_.get(); }

    static constexpr char const* SIGMA_Y_M_NAME = "sigma_y_m";
    inline void clear_sigma_y_m() { sigma_y_m_.clear(); }
    inline void set_sigma_y_m(const float& value) { sigma_y_m_ = value; }
    inline void set_sigma_y_m(const float&& value) { sigma_y_m_ = value; }
    inline float& mutable_sigma_y_m() { return sigma_y_m_.get(); }
    inline const float& get_sigma_y_m() const { return sigma_y_m_.get(); }
    inline float sigma_y_m() const { return sigma_y_m_.get(); }

    static constexpr char const* SIGMA_YAW_RAD_NAME = "sigma_yaw_rad";
    inline void clear_sigma_yaw_rad() { sigma_yaw_rad_.clear(); }
    inline void set_sigma_yaw_rad(const float& value) { sigma_yaw_rad_ = value; }
    inline void set_sigma_yaw_rad(const float&& value) { sigma_yaw_rad_ = value; }
    inline float& mutable_sigma_yaw_rad() { return sigma_yaw_rad_.get(); }
    inline const float& get_sigma_yaw_rad() const { return sigma_yaw_rad_.get(); }
    inline float sigma_yaw_rad() const { return sigma_yaw_rad_.get(); }

    static constexpr char const* GRID_X_NAME = "grid_x";
    inline void clear_grid_x() { grid_x_.clear(); }
    inline void set_grid_x(const int32_t& value) { grid_x_ = value; }
    inline void set_grid_x(const int32_t&& value) { grid_x_ = value; }
    inline int32_t& mutable_grid_x() { return grid_x_.get(); }
    inline const int32_t& get_grid_x() const { return grid_x_.get(); }
    inline int32_t grid_x() const { return grid_x_.get(); }

    static constexpr char const* GRID_Y_NAME = "grid_y";
    inline void clear_grid_y() { grid_y_.clear(); }
    inline void set_grid_y(const int32_t& value) { grid_y_ = value; }
    inline void set_grid_y(const int32_t&& value) { grid_y_ = value; }
    inline int32_t& mutable_grid_y() { return grid_y_.get(); }
    inline const int32_t& get_grid_y() const { return grid_y_.get(); }
    inline int32_t grid_y() const { return grid_y_.get(); }

    static constexpr char const* TRACKING_NAME = "tracking";
    inline void clear_tracking() { tracking_.clear(); }
    inline void set_tracking(const bool& value) { tracking_ = value; }
    inline void set_tracking(const bool&& value) { tracking_ = value; }
    inline bool& mutable_tracking() { return tracking_.get(); }
    inline const bool& get_tracking() const { return tracking_.get(); }
    inline bool tracking() const { return tracking_.get(); }

    static constexpr char const* GRID_CONFIDENT_NAME = "grid_confident";
    inline void clear_grid_confident() { grid_confident_.clear(); }
    inline void set_grid_confident(const bool& value) { grid_confident_ = value; }
    inline void set_grid_confident(const bool&& value) { grid_confident_ = value; }
    inline bool& mutable_grid_confident() { return grid_confident_.get(); }
    inline const bool& get_grid_confident() const { return grid_confident_.get(); }
    inline bool grid_confident() const { return grid_confident_.get(); }

    static constexpr char const* QUEUE_DROPPED_NAME = "queue_dropped";
    inline void clear_queue_dropped() { queue_dropped_.clear(); }
    inline void set_queue_dropped(const uint32_t& value) { queue_dropped_ = value; }
    inline void set_queue_dropped(const uint32_t&& value) { queue_dropped_ = value; }
    inline uint32_t& mutable_queue_dropped() { return queue_dropped_.get(); }
    inline const uint32_t& get_queue_dropped() const { return queue_dropped_.get(); }
    inline uint32_t queue_dropped() const { return queue_dropped_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != boot_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = boot_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::BOOT_ID), buffer, false);
      }

      if((0U != monotonic_us_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = monotonic_us_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MONOTONIC_US), buffer, false);
      }

      if((0U != sequence_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = sequence_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SEQUENCE), buffer, false);
      }

      if((0.0f != x_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = x_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::X_M), buffer, false);
      }

      if((0.0f != y_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = y_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::Y_M), buffer, false);
      }

      if((0.0f != yaw_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = yaw_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::YAW_RAD), buffer, false);
      }

      if((0.0f != gyro_bias_rps_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = gyro_bias_rps_.serialize_with_id(static_cast<uint32_t>(FieldNumber::GYRO_BIAS_RPS), buffer, false);
      }

      if((0.0f != sigma_x_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = sigma_x_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SIGMA_X_M), buffer, false);
      }

      if((0.0f != sigma_y_m_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = sigma_y_m_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SIGMA_Y_M), buffer, false);
      }

      if((0.0f != sigma_yaw_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = sigma_yaw_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SIGMA_YAW_RAD), buffer, false);
      }

      if((0 != grid_x_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = grid_x_.serialize_with_id(static_cast<uint32_t>(FieldNumber::GRID_X), buffer, false);
      }

      if((0 != grid_y_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = grid_y_.serialize_with_id(static_cast<uint32_t>(FieldNumber::GRID_Y), buffer, false);
      }

      if((false != tracking_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = tracking_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TRACKING), buffer, false);
      }

      if((false != grid_confident_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = grid_confident_.serialize_with_id(static_cast<uint32_t>(FieldNumber::GRID_CONFIDENT), buffer, false);
      }

      if((0U != queue_dropped_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = queue_dropped_.serialize_with_id(static_cast<uint32_t>(FieldNumber::QUEUE_DROPPED), buffer, false);
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
          case FieldNumber::BOOT_ID:
            return_value = boot_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MONOTONIC_US:
            return_value = monotonic_us_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::SEQUENCE:
            return_value = sequence_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::X_M:
            return_value = x_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::Y_M:
            return_value = y_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::YAW_RAD:
            return_value = yaw_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::GYRO_BIAS_RPS:
            return_value = gyro_bias_rps_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::SIGMA_X_M:
            return_value = sigma_x_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::SIGMA_Y_M:
            return_value = sigma_y_m_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::SIGMA_YAW_RAD:
            return_value = sigma_yaw_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::GRID_X:
            return_value = grid_x_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::GRID_Y:
            return_value = grid_y_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::TRACKING:
            return_value = tracking_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::GRID_CONFIDENT:
            return_value = grid_confident_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::QUEUE_DROPPED:
            return_value = queue_dropped_.deserialize_check_type(buffer, wire_type);
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
      clear_boot_id();
      clear_monotonic_us();
      clear_sequence();
      clear_x_m();
      clear_y_m();
      clear_yaw_rad();
      clear_gyro_bias_rps();
      clear_sigma_x_m();
      clear_sigma_y_m();
      clear_sigma_yaw_rad();
      clear_grid_x();
      clear_grid_y();
      clear_tracking();
      clear_grid_confident();
      clear_queue_dropped();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME 

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::BOOT_ID:
          name = BOOT_ID_NAME;
          break;
        case FieldNumber::MONOTONIC_US:
          name = MONOTONIC_US_NAME;
          break;
        case FieldNumber::SEQUENCE:
          name = SEQUENCE_NAME;
          break;
        case FieldNumber::X_M:
          name = X_M_NAME;
          break;
        case FieldNumber::Y_M:
          name = Y_M_NAME;
          break;
        case FieldNumber::YAW_RAD:
          name = YAW_RAD_NAME;
          break;
        case FieldNumber::GYRO_BIAS_RPS:
          name = GYRO_BIAS_RPS_NAME;
          break;
        case FieldNumber::SIGMA_X_M:
          name = SIGMA_X_M_NAME;
          break;
        case FieldNumber::SIGMA_Y_M:
          name = SIGMA_Y_M_NAME;
          break;
        case FieldNumber::SIGMA_YAW_RAD:
          name = SIGMA_YAW_RAD_NAME;
          break;
        case FieldNumber::GRID_X:
          name = GRID_X_NAME;
          break;
        case FieldNumber::GRID_Y:
          name = GRID_Y_NAME;
          break;
        case FieldNumber::TRACKING:
          name = TRACKING_NAME;
          break;
        case FieldNumber::GRID_CONFIDENT:
          name = GRID_CONFIDENT_NAME;
          break;
        case FieldNumber::QUEUE_DROPPED:
          name = QUEUE_DROPPED_NAME;
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
          EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::BOOT_ID))
          + EmbeddedProto::uint64::max_serialized_size(static_cast<uint32_t>(FieldNumber::MONOTONIC_US))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::SEQUENCE))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::X_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::Y_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::YAW_RAD))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::GYRO_BIAS_RPS))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::SIGMA_X_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::SIGMA_Y_M))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::SIGMA_YAW_RAD))
          + EmbeddedProto::sint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::GRID_X))
          + EmbeddedProto::sint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::GRID_Y))
          + EmbeddedProto::boolean::max_serialized_size(static_cast<uint32_t>(FieldNumber::TRACKING))
          + EmbeddedProto::boolean::max_serialized_size(static_cast<uint32_t>(FieldNumber::GRID_CONFIDENT))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::QUEUE_DROPPED))
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

      left_chars = boot_id_.to_string(left_chars, indent_level + 2, BOOT_ID_NAME, true);
      left_chars = monotonic_us_.to_string(left_chars, indent_level + 2, MONOTONIC_US_NAME, false);
      left_chars = sequence_.to_string(left_chars, indent_level + 2, SEQUENCE_NAME, false);
      left_chars = x_m_.to_string(left_chars, indent_level + 2, X_M_NAME, false);
      left_chars = y_m_.to_string(left_chars, indent_level + 2, Y_M_NAME, false);
      left_chars = yaw_rad_.to_string(left_chars, indent_level + 2, YAW_RAD_NAME, false);
      left_chars = gyro_bias_rps_.to_string(left_chars, indent_level + 2, GYRO_BIAS_RPS_NAME, false);
      left_chars = sigma_x_m_.to_string(left_chars, indent_level + 2, SIGMA_X_M_NAME, false);
      left_chars = sigma_y_m_.to_string(left_chars, indent_level + 2, SIGMA_Y_M_NAME, false);
      left_chars = sigma_yaw_rad_.to_string(left_chars, indent_level + 2, SIGMA_YAW_RAD_NAME, false);
      left_chars = grid_x_.to_string(left_chars, indent_level + 2, GRID_X_NAME, false);
      left_chars = grid_y_.to_string(left_chars, indent_level + 2, GRID_Y_NAME, false);
      left_chars = tracking_.to_string(left_chars, indent_level + 2, TRACKING_NAME, false);
      left_chars = grid_confident_.to_string(left_chars, indent_level + 2, GRID_CONFIDENT_NAME, false);
      left_chars = queue_dropped_.to_string(left_chars, indent_level + 2, QUEUE_DROPPED_NAME, false);
  
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


      EmbeddedProto::uint32 boot_id_ = 0U;
      EmbeddedProto::uint64 monotonic_us_ = 0U;
      EmbeddedProto::uint32 sequence_ = 0U;
      EmbeddedProto::floatfixed x_m_ = 0.0f;
      EmbeddedProto::floatfixed y_m_ = 0.0f;
      EmbeddedProto::floatfixed yaw_rad_ = 0.0f;
      EmbeddedProto::floatfixed gyro_bias_rps_ = 0.0f;
      EmbeddedProto::floatfixed sigma_x_m_ = 0.0f;
      EmbeddedProto::floatfixed sigma_y_m_ = 0.0f;
      EmbeddedProto::floatfixed sigma_yaw_rad_ = 0.0f;
      EmbeddedProto::sint32 grid_x_ = 0;
      EmbeddedProto::sint32 grid_y_ = 0;
      EmbeddedProto::boolean tracking_ = false;
      EmbeddedProto::boolean grid_confident_ = false;
      EmbeddedProto::uint32 queue_dropped_ = 0U;

};

template<
    uint32_t LinkInfo_ipv6_linklocal_LENGTH, 
    uint32_t LinkInfo_certificate_sha256_LENGTH, 
    uint32_t LinkInfo_device_id_LENGTH
>
class LinkInfo final: public ::EmbeddedProto::MessageInterface
{
  public:
    LinkInfo() = default;
    LinkInfo(const LinkInfo& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_ipv6_linklocal(rhs.get_ipv6_linklocal());
      set_certificate_sha256(rhs.get_certificate_sha256());
      set_port(rhs.get_port());
      set_device_id(rhs.get_device_id());
    }

    LinkInfo(const LinkInfo&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_ipv6_linklocal(rhs.get_ipv6_linklocal());
      set_certificate_sha256(rhs.get_certificate_sha256());
      set_port(rhs.get_port());
      set_device_id(rhs.get_device_id());
    }

    ~LinkInfo() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      IPV6_LINKLOCAL = 2,
      CERTIFICATE_SHA256 = 3,
      PORT = 4,
      DEVICE_ID = 5
    };

    LinkInfo& operator=(const LinkInfo& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_ipv6_linklocal(rhs.get_ipv6_linklocal());
      set_certificate_sha256(rhs.get_certificate_sha256());
      set_port(rhs.get_port());
      set_device_id(rhs.get_device_id());
      return *this;
    }

    LinkInfo& operator=(const LinkInfo&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_ipv6_linklocal(rhs.get_ipv6_linklocal());
      set_certificate_sha256(rhs.get_certificate_sha256());
      set_port(rhs.get_port());
      set_device_id(rhs.get_device_id());
      return *this;
    }

    static constexpr char const* REQUEST_ID_NAME = "request_id";
    inline void clear_request_id() { request_id_.clear(); }
    inline void set_request_id(const uint32_t& value) { request_id_ = value; }
    inline void set_request_id(const uint32_t&& value) { request_id_ = value; }
    inline uint32_t& mutable_request_id() { return request_id_.get(); }
    inline const uint32_t& get_request_id() const { return request_id_.get(); }
    inline uint32_t request_id() const { return request_id_.get(); }

    static constexpr char const* IPV6_LINKLOCAL_NAME = "ipv6_linklocal";
    inline void clear_ipv6_linklocal() { ipv6_linklocal_.clear(); }
    inline ::EmbeddedProto::FieldString<LinkInfo_ipv6_linklocal_LENGTH>& mutable_ipv6_linklocal() { return ipv6_linklocal_; }
    inline void set_ipv6_linklocal(const ::EmbeddedProto::FieldString<LinkInfo_ipv6_linklocal_LENGTH>& rhs) { ipv6_linklocal_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<LinkInfo_ipv6_linklocal_LENGTH>& get_ipv6_linklocal() const { return ipv6_linklocal_; }
    inline const char* ipv6_linklocal() const { return ipv6_linklocal_.get_const(); }

    static constexpr char const* CERTIFICATE_SHA256_NAME = "certificate_sha256";
    inline void clear_certificate_sha256() { certificate_sha256_.clear(); }
    inline ::EmbeddedProto::FieldString<LinkInfo_certificate_sha256_LENGTH>& mutable_certificate_sha256() { return certificate_sha256_; }
    inline void set_certificate_sha256(const ::EmbeddedProto::FieldString<LinkInfo_certificate_sha256_LENGTH>& rhs) { certificate_sha256_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<LinkInfo_certificate_sha256_LENGTH>& get_certificate_sha256() const { return certificate_sha256_; }
    inline const char* certificate_sha256() const { return certificate_sha256_.get_const(); }

    static constexpr char const* PORT_NAME = "port";
    inline void clear_port() { port_.clear(); }
    inline void set_port(const uint32_t& value) { port_ = value; }
    inline void set_port(const uint32_t&& value) { port_ = value; }
    inline uint32_t& mutable_port() { return port_.get(); }
    inline const uint32_t& get_port() const { return port_.get(); }
    inline uint32_t port() const { return port_.get(); }

    static constexpr char const* DEVICE_ID_NAME = "device_id";
    inline void clear_device_id() { device_id_.clear(); }
    inline ::EmbeddedProto::FieldString<LinkInfo_device_id_LENGTH>& mutable_device_id() { return device_id_; }
    inline void set_device_id(const ::EmbeddedProto::FieldString<LinkInfo_device_id_LENGTH>& rhs) { device_id_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<LinkInfo_device_id_LENGTH>& get_device_id() const { return device_id_; }
    inline const char* device_id() const { return device_id_.get_const(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = ipv6_linklocal_.serialize_with_id(static_cast<uint32_t>(FieldNumber::IPV6_LINKLOCAL), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = certificate_sha256_.serialize_with_id(static_cast<uint32_t>(FieldNumber::CERTIFICATE_SHA256), buffer, false);
      }

      if((0U != port_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = port_.serialize_with_id(static_cast<uint32_t>(FieldNumber::PORT), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = device_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::DEVICE_ID), buffer, false);
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
          case FieldNumber::REQUEST_ID:
            return_value = request_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::IPV6_LINKLOCAL:
            return_value = ipv6_linklocal_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::CERTIFICATE_SHA256:
            return_value = certificate_sha256_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::PORT:
            return_value = port_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::DEVICE_ID:
            return_value = device_id_.deserialize_check_type(buffer, wire_type);
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
      clear_request_id();
      clear_ipv6_linklocal();
      clear_certificate_sha256();
      clear_port();
      clear_device_id();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME 

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::REQUEST_ID:
          name = REQUEST_ID_NAME;
          break;
        case FieldNumber::IPV6_LINKLOCAL:
          name = IPV6_LINKLOCAL_NAME;
          break;
        case FieldNumber::CERTIFICATE_SHA256:
          name = CERTIFICATE_SHA256_NAME;
          break;
        case FieldNumber::PORT:
          name = PORT_NAME;
          break;
        case FieldNumber::DEVICE_ID:
          name = DEVICE_ID_NAME;
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
          EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::REQUEST_ID))
          + ::EmbeddedProto::FieldString<LinkInfo_ipv6_linklocal_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::IPV6_LINKLOCAL))
          + ::EmbeddedProto::FieldString<LinkInfo_certificate_sha256_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::CERTIFICATE_SHA256))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::PORT))
          + ::EmbeddedProto::FieldString<LinkInfo_device_id_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::DEVICE_ID))
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

      left_chars = request_id_.to_string(left_chars, indent_level + 2, REQUEST_ID_NAME, true);
      left_chars = ipv6_linklocal_.to_string(left_chars, indent_level + 2, IPV6_LINKLOCAL_NAME, false);
      left_chars = certificate_sha256_.to_string(left_chars, indent_level + 2, CERTIFICATE_SHA256_NAME, false);
      left_chars = port_.to_string(left_chars, indent_level + 2, PORT_NAME, false);
      left_chars = device_id_.to_string(left_chars, indent_level + 2, DEVICE_ID_NAME, false);
  
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


      EmbeddedProto::uint32 request_id_ = 0U;
      ::EmbeddedProto::FieldString<LinkInfo_ipv6_linklocal_LENGTH> ipv6_linklocal_;
      ::EmbeddedProto::FieldString<LinkInfo_certificate_sha256_LENGTH> certificate_sha256_;
      EmbeddedProto::uint32 port_ = 0U;
      ::EmbeddedProto::FieldString<LinkInfo_device_id_LENGTH> device_id_;

};

template<
    uint32_t LogRecord_source_LENGTH, 
    uint32_t LogRecord_message_LENGTH
>
class LogRecord final: public ::EmbeddedProto::MessageInterface
{
  public:
    LogRecord() = default;
    LogRecord(const LogRecord& rhs )
    {
      set_timestamp_ms(rhs.get_timestamp_ms());
      set_level(rhs.get_level());
      set_source(rhs.get_source());
      set_message(rhs.get_message());
      set_collector_dropped(rhs.get_collector_dropped());
    }

    LogRecord(const LogRecord&& rhs ) noexcept
    {
      set_timestamp_ms(rhs.get_timestamp_ms());
      set_level(rhs.get_level());
      set_source(rhs.get_source());
      set_message(rhs.get_message());
      set_collector_dropped(rhs.get_collector_dropped());
    }

    ~LogRecord() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      TIMESTAMP_MS = 1,
      LEVEL = 2,
      SOURCE = 3,
      MESSAGE = 4,
      COLLECTOR_DROPPED = 5
    };

    LogRecord& operator=(const LogRecord& rhs)
    {
      set_timestamp_ms(rhs.get_timestamp_ms());
      set_level(rhs.get_level());
      set_source(rhs.get_source());
      set_message(rhs.get_message());
      set_collector_dropped(rhs.get_collector_dropped());
      return *this;
    }

    LogRecord& operator=(const LogRecord&& rhs) noexcept
    {
      set_timestamp_ms(rhs.get_timestamp_ms());
      set_level(rhs.get_level());
      set_source(rhs.get_source());
      set_message(rhs.get_message());
      set_collector_dropped(rhs.get_collector_dropped());
      return *this;
    }

    static constexpr char const* TIMESTAMP_MS_NAME = "timestamp_ms";
    inline void clear_timestamp_ms() { timestamp_ms_.clear(); }
    inline void set_timestamp_ms(const uint32_t& value) { timestamp_ms_ = value; }
    inline void set_timestamp_ms(const uint32_t&& value) { timestamp_ms_ = value; }
    inline uint32_t& mutable_timestamp_ms() { return timestamp_ms_.get(); }
    inline const uint32_t& get_timestamp_ms() const { return timestamp_ms_.get(); }
    inline uint32_t timestamp_ms() const { return timestamp_ms_.get(); }

    static constexpr char const* LEVEL_NAME = "level";
    inline void clear_level() { level_.clear(); }
    inline void set_level(const uint32_t& value) { level_ = value; }
    inline void set_level(const uint32_t&& value) { level_ = value; }
    inline uint32_t& mutable_level() { return level_.get(); }
    inline const uint32_t& get_level() const { return level_.get(); }
    inline uint32_t level() const { return level_.get(); }

    static constexpr char const* SOURCE_NAME = "source";
    inline void clear_source() { source_.clear(); }
    inline ::EmbeddedProto::FieldString<LogRecord_source_LENGTH>& mutable_source() { return source_; }
    inline void set_source(const ::EmbeddedProto::FieldString<LogRecord_source_LENGTH>& rhs) { source_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<LogRecord_source_LENGTH>& get_source() const { return source_; }
    inline const char* source() const { return source_.get_const(); }

    static constexpr char const* MESSAGE_NAME = "message";
    inline void clear_message() { message_.clear(); }
    inline ::EmbeddedProto::FieldString<LogRecord_message_LENGTH>& mutable_message() { return message_; }
    inline void set_message(const ::EmbeddedProto::FieldString<LogRecord_message_LENGTH>& rhs) { message_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<LogRecord_message_LENGTH>& get_message() const { return message_; }
    inline const char* message() const { return message_.get_const(); }

    static constexpr char const* COLLECTOR_DROPPED_NAME = "collector_dropped";
    inline void clear_collector_dropped() { collector_dropped_.clear(); }
    inline void set_collector_dropped(const uint32_t& value) { collector_dropped_ = value; }
    inline void set_collector_dropped(const uint32_t&& value) { collector_dropped_ = value; }
    inline uint32_t& mutable_collector_dropped() { return collector_dropped_.get(); }
    inline const uint32_t& get_collector_dropped() const { return collector_dropped_.get(); }
    inline uint32_t collector_dropped() const { return collector_dropped_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != timestamp_ms_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = timestamp_ms_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TIMESTAMP_MS), buffer, false);
      }

      if((0U != level_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = level_.serialize_with_id(static_cast<uint32_t>(FieldNumber::LEVEL), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = source_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SOURCE), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = message_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MESSAGE), buffer, false);
      }

      if((0U != collector_dropped_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = collector_dropped_.serialize_with_id(static_cast<uint32_t>(FieldNumber::COLLECTOR_DROPPED), buffer, false);
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
          case FieldNumber::TIMESTAMP_MS:
            return_value = timestamp_ms_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::LEVEL:
            return_value = level_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::SOURCE:
            return_value = source_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MESSAGE:
            return_value = message_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::COLLECTOR_DROPPED:
            return_value = collector_dropped_.deserialize_check_type(buffer, wire_type);
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
      clear_timestamp_ms();
      clear_level();
      clear_source();
      clear_message();
      clear_collector_dropped();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME 

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::TIMESTAMP_MS:
          name = TIMESTAMP_MS_NAME;
          break;
        case FieldNumber::LEVEL:
          name = LEVEL_NAME;
          break;
        case FieldNumber::SOURCE:
          name = SOURCE_NAME;
          break;
        case FieldNumber::MESSAGE:
          name = MESSAGE_NAME;
          break;
        case FieldNumber::COLLECTOR_DROPPED:
          name = COLLECTOR_DROPPED_NAME;
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
          EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::TIMESTAMP_MS))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::LEVEL))
          + ::EmbeddedProto::FieldString<LogRecord_source_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::SOURCE))
          + ::EmbeddedProto::FieldString<LogRecord_message_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::MESSAGE))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::COLLECTOR_DROPPED))
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

      left_chars = timestamp_ms_.to_string(left_chars, indent_level + 2, TIMESTAMP_MS_NAME, true);
      left_chars = level_.to_string(left_chars, indent_level + 2, LEVEL_NAME, false);
      left_chars = source_.to_string(left_chars, indent_level + 2, SOURCE_NAME, false);
      left_chars = message_.to_string(left_chars, indent_level + 2, MESSAGE_NAME, false);
      left_chars = collector_dropped_.to_string(left_chars, indent_level + 2, COLLECTOR_DROPPED_NAME, false);
  
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


      EmbeddedProto::uint32 timestamp_ms_ = 0U;
      EmbeddedProto::uint32 level_ = 0U;
      ::EmbeddedProto::FieldString<LogRecord_source_LENGTH> source_;
      ::EmbeddedProto::FieldString<LogRecord_message_LENGTH> message_;
      EmbeddedProto::uint32 collector_dropped_ = 0U;

};

} // End of namespace v1
} // End of namespace telemetry
} // End of namespace axi
#endif // TELEMETRY_H