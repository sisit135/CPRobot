/*
 *  This file is generated with Embedded Proto, PLEASE DO NOT EDIT!
 *  source: remote_config.proto
 */

// This file is generated. Please do not edit!
#ifndef REMOTE_CONFIG_H
#define REMOTE_CONFIG_H

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
#include "telemetry.h"

namespace axi {
namespace remote {
namespace v1 {

enum class Operation : uint32_t
{
  UNSPECIFIED = 0,
  GET_SCHEMA = 1,
  LIST_FIELDS = 2,
  READ = 3,
  WRITE = 4,
  CALIBRATE_GYRO = 5,
  GET_SOURCE = 6
};

enum class Status : uint32_t
{
  OK = 0,
  INVALID = 1,
  CONFLICT = 2,
  BUSY = 3,
  IO_ERROR = 4,
  NOT_FOUND = 5,
  NOT_READY = 6
};

enum class ValueType : uint32_t
{
  FLOAT = 0,
  INTEGER = 1,
  BOOLEAN = 2
};

class CalibrateColorRequest final: public ::EmbeddedProto::MessageInterface
{
  public:
    CalibrateColorRequest() = default;
    CalibrateColorRequest(const CalibrateColorRequest& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_color_id_mapped(rhs.get_color_id_mapped());
    }

    CalibrateColorRequest(const CalibrateColorRequest&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_color_id_mapped(rhs.get_color_id_mapped());
    }

    ~CalibrateColorRequest() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      COLOR_ID_MAPPED = 2
    };

    CalibrateColorRequest& operator=(const CalibrateColorRequest& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_color_id_mapped(rhs.get_color_id_mapped());
      return *this;
    }

    CalibrateColorRequest& operator=(const CalibrateColorRequest&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_color_id_mapped(rhs.get_color_id_mapped());
      return *this;
    }

    static constexpr char const* REQUEST_ID_NAME = "request_id";
    inline void clear_request_id() { request_id_.clear(); }
    inline void set_request_id(const uint32_t& value) { request_id_ = value; }
    inline void set_request_id(const uint32_t&& value) { request_id_ = value; }
    inline uint32_t& mutable_request_id() { return request_id_.get(); }
    inline const uint32_t& get_request_id() const { return request_id_.get(); }
    inline uint32_t request_id() const { return request_id_.get(); }

    static constexpr char const* COLOR_ID_MAPPED_NAME = "color_id_mapped";
    inline void clear_color_id_mapped() { color_id_mapped_.clear(); }
    inline void set_color_id_mapped(const uint32_t& value) { color_id_mapped_ = value; }
    inline void set_color_id_mapped(const uint32_t&& value) { color_id_mapped_ = value; }
    inline uint32_t& mutable_color_id_mapped() { return color_id_mapped_.get(); }
    inline const uint32_t& get_color_id_mapped() const { return color_id_mapped_.get(); }
    inline uint32_t color_id_mapped() const { return color_id_mapped_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if((0U != color_id_mapped_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = color_id_mapped_.serialize_with_id(static_cast<uint32_t>(FieldNumber::COLOR_ID_MAPPED), buffer, false);
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

          case FieldNumber::COLOR_ID_MAPPED:
            return_value = color_id_mapped_.deserialize_check_type(buffer, wire_type);
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
      clear_color_id_mapped();

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
        case FieldNumber::COLOR_ID_MAPPED:
          name = COLOR_ID_MAPPED_NAME;
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
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::COLOR_ID_MAPPED))
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
      left_chars = color_id_mapped_.to_string(left_chars, indent_level + 2, COLOR_ID_MAPPED_NAME, false);
  
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
      EmbeddedProto::uint32 color_id_mapped_ = 0U;

};

template<
    uint32_t FieldSpec_name_LENGTH, 
    uint32_t FieldSpec_group_LENGTH, 
    uint32_t FieldSpec_source_LENGTH
>
class FieldSpec final: public ::EmbeddedProto::MessageInterface
{
  public:
    FieldSpec() = default;
    FieldSpec(const FieldSpec& rhs )
    {
      set_key(rhs.get_key());
      set_name(rhs.get_name());
      set_group(rhs.get_group());
      set_type(rhs.get_type());
      set_minimum(rhs.get_minimum());
      set_maximum(rhs.get_maximum());
      set_source(rhs.get_source());
    }

    FieldSpec(const FieldSpec&& rhs ) noexcept
    {
      set_key(rhs.get_key());
      set_name(rhs.get_name());
      set_group(rhs.get_group());
      set_type(rhs.get_type());
      set_minimum(rhs.get_minimum());
      set_maximum(rhs.get_maximum());
      set_source(rhs.get_source());
    }

    ~FieldSpec() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      KEY = 1,
      NAME = 2,
      GROUP = 3,
      TYPE = 4,
      MINIMUM = 5,
      MAXIMUM = 6,
      SOURCE = 7
    };

    FieldSpec& operator=(const FieldSpec& rhs)
    {
      set_key(rhs.get_key());
      set_name(rhs.get_name());
      set_group(rhs.get_group());
      set_type(rhs.get_type());
      set_minimum(rhs.get_minimum());
      set_maximum(rhs.get_maximum());
      set_source(rhs.get_source());
      return *this;
    }

    FieldSpec& operator=(const FieldSpec&& rhs) noexcept
    {
      set_key(rhs.get_key());
      set_name(rhs.get_name());
      set_group(rhs.get_group());
      set_type(rhs.get_type());
      set_minimum(rhs.get_minimum());
      set_maximum(rhs.get_maximum());
      set_source(rhs.get_source());
      return *this;
    }

    static constexpr char const* KEY_NAME = "key";
    inline void clear_key() { key_.clear(); }
    inline void set_key(const uint32_t& value) { key_ = value; }
    inline void set_key(const uint32_t&& value) { key_ = value; }
    inline uint32_t& mutable_key() { return key_.get(); }
    inline const uint32_t& get_key() const { return key_.get(); }
    inline uint32_t key() const { return key_.get(); }

    static constexpr char const* NAME_NAME = "name";
    inline void clear_name() { name_.clear(); }
    inline ::EmbeddedProto::FieldString<FieldSpec_name_LENGTH>& mutable_name() { return name_; }
    inline void set_name(const ::EmbeddedProto::FieldString<FieldSpec_name_LENGTH>& rhs) { name_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<FieldSpec_name_LENGTH>& get_name() const { return name_; }
    inline const char* name() const { return name_.get_const(); }

    static constexpr char const* GROUP_NAME = "group";
    inline void clear_group() { group_.clear(); }
    inline ::EmbeddedProto::FieldString<FieldSpec_group_LENGTH>& mutable_group() { return group_; }
    inline void set_group(const ::EmbeddedProto::FieldString<FieldSpec_group_LENGTH>& rhs) { group_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<FieldSpec_group_LENGTH>& get_group() const { return group_; }
    inline const char* group() const { return group_.get_const(); }

    static constexpr char const* TYPE_NAME = "type";
    inline void clear_type() { type_.clear(); }
    inline void set_type(const ValueType& value) { type_ = value; }
    inline void set_type(const ValueType&& value) { type_ = value; }
    inline const ValueType& get_type() const { return type_.get(); }
    inline ValueType type() const { return type_.get(); }

    static constexpr char const* MINIMUM_NAME = "minimum";
    inline void clear_minimum() { minimum_.clear(); }
    inline void set_minimum(const float& value) { minimum_ = value; }
    inline void set_minimum(const float&& value) { minimum_ = value; }
    inline float& mutable_minimum() { return minimum_.get(); }
    inline const float& get_minimum() const { return minimum_.get(); }
    inline float minimum() const { return minimum_.get(); }

    static constexpr char const* MAXIMUM_NAME = "maximum";
    inline void clear_maximum() { maximum_.clear(); }
    inline void set_maximum(const float& value) { maximum_ = value; }
    inline void set_maximum(const float&& value) { maximum_ = value; }
    inline float& mutable_maximum() { return maximum_.get(); }
    inline const float& get_maximum() const { return maximum_.get(); }
    inline float maximum() const { return maximum_.get(); }

    static constexpr char const* SOURCE_NAME = "source";
    inline void clear_source() { source_.clear(); }
    inline ::EmbeddedProto::FieldString<FieldSpec_source_LENGTH>& mutable_source() { return source_; }
    inline void set_source(const ::EmbeddedProto::FieldString<FieldSpec_source_LENGTH>& rhs) { source_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<FieldSpec_source_LENGTH>& get_source() const { return source_; }
    inline const char* source() const { return source_.get_const(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != key_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = key_.serialize_with_id(static_cast<uint32_t>(FieldNumber::KEY), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = name_.serialize_with_id(static_cast<uint32_t>(FieldNumber::NAME), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = group_.serialize_with_id(static_cast<uint32_t>(FieldNumber::GROUP), buffer, false);
      }

      if((static_cast<ValueType>(0) != type_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = type_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TYPE), buffer, false);
      }

      if((0.0f != minimum_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = minimum_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MINIMUM), buffer, false);
      }

      if((0.0f != maximum_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = maximum_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MAXIMUM), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = source_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SOURCE), buffer, false);
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
          case FieldNumber::KEY:
            return_value = key_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NAME:
            return_value = name_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::GROUP:
            return_value = group_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::TYPE:
            return_value = type_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MINIMUM:
            return_value = minimum_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MAXIMUM:
            return_value = maximum_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::SOURCE:
            return_value = source_.deserialize_check_type(buffer, wire_type);
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
      clear_key();
      clear_name();
      clear_group();
      clear_type();
      clear_minimum();
      clear_maximum();
      clear_source();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME 

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::KEY:
          name = KEY_NAME;
          break;
        case FieldNumber::NAME:
          name = NAME_NAME;
          break;
        case FieldNumber::GROUP:
          name = GROUP_NAME;
          break;
        case FieldNumber::TYPE:
          name = TYPE_NAME;
          break;
        case FieldNumber::MINIMUM:
          name = MINIMUM_NAME;
          break;
        case FieldNumber::MAXIMUM:
          name = MAXIMUM_NAME;
          break;
        case FieldNumber::SOURCE:
          name = SOURCE_NAME;
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
          EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::KEY))
          + ::EmbeddedProto::FieldString<FieldSpec_name_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::NAME))
          + ::EmbeddedProto::FieldString<FieldSpec_group_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::GROUP))
          + EmbeddedProto::enumeration<ValueType, EmbeddedProto::WireFormatter::VarintSize(2)>::max_serialized_size(static_cast<uint32_t>(FieldNumber::TYPE))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::MINIMUM))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::MAXIMUM))
          + ::EmbeddedProto::FieldString<FieldSpec_source_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::SOURCE))
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

      left_chars = key_.to_string(left_chars, indent_level + 2, KEY_NAME, true);
      left_chars = name_.to_string(left_chars, indent_level + 2, NAME_NAME, false);
      left_chars = group_.to_string(left_chars, indent_level + 2, GROUP_NAME, false);
      left_chars = type_.to_string(left_chars, indent_level + 2, TYPE_NAME, false);
      left_chars = minimum_.to_string(left_chars, indent_level + 2, MINIMUM_NAME, false);
      left_chars = maximum_.to_string(left_chars, indent_level + 2, MAXIMUM_NAME, false);
      left_chars = source_.to_string(left_chars, indent_level + 2, SOURCE_NAME, false);
  
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


      EmbeddedProto::uint32 key_ = 0U;
      ::EmbeddedProto::FieldString<FieldSpec_name_LENGTH> name_;
      ::EmbeddedProto::FieldString<FieldSpec_group_LENGTH> group_;
      EmbeddedProto::enumeration<ValueType, EmbeddedProto::WireFormatter::VarintSize(2)> type_ = static_cast<ValueType>(0);
      EmbeddedProto::floatfixed minimum_ = 0.0f;
      EmbeddedProto::floatfixed maximum_ = 0.0f;
      ::EmbeddedProto::FieldString<FieldSpec_source_LENGTH> source_;

};

class RemoteRequest final: public ::EmbeddedProto::MessageInterface
{
  public:
    RemoteRequest() = default;
    RemoteRequest(const RemoteRequest& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      set_key(rhs.get_key());
      set_value(rhs.get_value());
      set_expected_revision(rhs.get_expected_revision());
      set_offset(rhs.get_offset());
    }

    RemoteRequest(const RemoteRequest&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      set_key(rhs.get_key());
      set_value(rhs.get_value());
      set_expected_revision(rhs.get_expected_revision());
      set_offset(rhs.get_offset());
    }

    ~RemoteRequest() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      OPERATION = 2,
      KEY = 3,
      VALUE = 4,
      EXPECTED_REVISION = 5,
      OFFSET = 6
    };

    RemoteRequest& operator=(const RemoteRequest& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      set_key(rhs.get_key());
      set_value(rhs.get_value());
      set_expected_revision(rhs.get_expected_revision());
      set_offset(rhs.get_offset());
      return *this;
    }

    RemoteRequest& operator=(const RemoteRequest&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_operation(rhs.get_operation());
      set_key(rhs.get_key());
      set_value(rhs.get_value());
      set_expected_revision(rhs.get_expected_revision());
      set_offset(rhs.get_offset());
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
    inline void set_operation(const Operation& value) { operation_ = value; }
    inline void set_operation(const Operation&& value) { operation_ = value; }
    inline const Operation& get_operation() const { return operation_.get(); }
    inline Operation operation() const { return operation_.get(); }

    static constexpr char const* KEY_NAME = "key";
    inline void clear_key() { key_.clear(); }
    inline void set_key(const uint32_t& value) { key_ = value; }
    inline void set_key(const uint32_t&& value) { key_ = value; }
    inline uint32_t& mutable_key() { return key_.get(); }
    inline const uint32_t& get_key() const { return key_.get(); }
    inline uint32_t key() const { return key_.get(); }

    static constexpr char const* VALUE_NAME = "value";
    inline void clear_value() { value_.clear(); }
    inline void set_value(const float& value) { value_ = value; }
    inline void set_value(const float&& value) { value_ = value; }
    inline float& mutable_value() { return value_.get(); }
    inline const float& get_value() const { return value_.get(); }
    inline float value() const { return value_.get(); }

    static constexpr char const* EXPECTED_REVISION_NAME = "expected_revision";
    inline void clear_expected_revision() { expected_revision_.clear(); }
    inline void set_expected_revision(const uint32_t& value) { expected_revision_ = value; }
    inline void set_expected_revision(const uint32_t&& value) { expected_revision_ = value; }
    inline uint32_t& mutable_expected_revision() { return expected_revision_.get(); }
    inline const uint32_t& get_expected_revision() const { return expected_revision_.get(); }
    inline uint32_t expected_revision() const { return expected_revision_.get(); }

    static constexpr char const* OFFSET_NAME = "offset";
    inline void clear_offset() { offset_.clear(); }
    inline void set_offset(const uint32_t& value) { offset_ = value; }
    inline void set_offset(const uint32_t&& value) { offset_ = value; }
    inline uint32_t& mutable_offset() { return offset_.get(); }
    inline const uint32_t& get_offset() const { return offset_.get(); }
    inline uint32_t offset() const { return offset_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if((static_cast<Operation>(0) != operation_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = operation_.serialize_with_id(static_cast<uint32_t>(FieldNumber::OPERATION), buffer, false);
      }

      if((0U != key_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = key_.serialize_with_id(static_cast<uint32_t>(FieldNumber::KEY), buffer, false);
      }

      if((0.0f != value_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = value_.serialize_with_id(static_cast<uint32_t>(FieldNumber::VALUE), buffer, false);
      }

      if((0U != expected_revision_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = expected_revision_.serialize_with_id(static_cast<uint32_t>(FieldNumber::EXPECTED_REVISION), buffer, false);
      }

      if((0U != offset_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = offset_.serialize_with_id(static_cast<uint32_t>(FieldNumber::OFFSET), buffer, false);
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

          case FieldNumber::KEY:
            return_value = key_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::VALUE:
            return_value = value_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::EXPECTED_REVISION:
            return_value = expected_revision_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::OFFSET:
            return_value = offset_.deserialize_check_type(buffer, wire_type);
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
      clear_key();
      clear_value();
      clear_expected_revision();
      clear_offset();

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
        case FieldNumber::KEY:
          name = KEY_NAME;
          break;
        case FieldNumber::VALUE:
          name = VALUE_NAME;
          break;
        case FieldNumber::EXPECTED_REVISION:
          name = EXPECTED_REVISION_NAME;
          break;
        case FieldNumber::OFFSET:
          name = OFFSET_NAME;
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
          + EmbeddedProto::enumeration<Operation, EmbeddedProto::WireFormatter::VarintSize(6)>::max_serialized_size(static_cast<uint32_t>(FieldNumber::OPERATION))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::KEY))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::VALUE))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::EXPECTED_REVISION))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::OFFSET))
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
      left_chars = key_.to_string(left_chars, indent_level + 2, KEY_NAME, false);
      left_chars = value_.to_string(left_chars, indent_level + 2, VALUE_NAME, false);
      left_chars = expected_revision_.to_string(left_chars, indent_level + 2, EXPECTED_REVISION_NAME, false);
      left_chars = offset_.to_string(left_chars, indent_level + 2, OFFSET_NAME, false);
  
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
      EmbeddedProto::enumeration<Operation, EmbeddedProto::WireFormatter::VarintSize(6)> operation_ = static_cast<Operation>(0);
      EmbeddedProto::uint32 key_ = 0U;
      EmbeddedProto::floatfixed value_ = 0.0f;
      EmbeddedProto::uint32 expected_revision_ = 0U;
      EmbeddedProto::uint32 offset_ = 0U;

};

template<
    uint32_t RemoteReply_field_FieldSpec_name_LENGTH, 
    uint32_t RemoteReply_field_FieldSpec_group_LENGTH, 
    uint32_t RemoteReply_field_FieldSpec_source_LENGTH, 
    uint32_t RemoteReply_schema_chunk_LENGTH
>
class RemoteReply final: public ::EmbeddedProto::MessageInterface
{
  public:
    RemoteReply() = default;
    RemoteReply(const RemoteReply& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_status(rhs.get_status());
      set_revision(rhs.get_revision());
      set_provisioned(rhs.get_provisioned());
      set_field(rhs.get_field());
      set_value(rhs.get_value());
      set_schema_chunk(rhs.get_schema_chunk());
      set_total(rhs.get_total());
      set_offset(rhs.get_offset());
    }

    RemoteReply(const RemoteReply&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_status(rhs.get_status());
      set_revision(rhs.get_revision());
      set_provisioned(rhs.get_provisioned());
      set_field(rhs.get_field());
      set_value(rhs.get_value());
      set_schema_chunk(rhs.get_schema_chunk());
      set_total(rhs.get_total());
      set_offset(rhs.get_offset());
    }

    ~RemoteReply() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      STATUS = 2,
      REVISION = 3,
      PROVISIONED = 4,
      FIELD = 5,
      VALUE = 6,
      SCHEMA_CHUNK = 7,
      TOTAL = 8,
      OFFSET = 9
    };

    RemoteReply& operator=(const RemoteReply& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_status(rhs.get_status());
      set_revision(rhs.get_revision());
      set_provisioned(rhs.get_provisioned());
      set_field(rhs.get_field());
      set_value(rhs.get_value());
      set_schema_chunk(rhs.get_schema_chunk());
      set_total(rhs.get_total());
      set_offset(rhs.get_offset());
      return *this;
    }

    RemoteReply& operator=(const RemoteReply&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_status(rhs.get_status());
      set_revision(rhs.get_revision());
      set_provisioned(rhs.get_provisioned());
      set_field(rhs.get_field());
      set_value(rhs.get_value());
      set_schema_chunk(rhs.get_schema_chunk());
      set_total(rhs.get_total());
      set_offset(rhs.get_offset());
      return *this;
    }

    static constexpr char const* REQUEST_ID_NAME = "request_id";
    inline void clear_request_id() { request_id_.clear(); }
    inline void set_request_id(const uint32_t& value) { request_id_ = value; }
    inline void set_request_id(const uint32_t&& value) { request_id_ = value; }
    inline uint32_t& mutable_request_id() { return request_id_.get(); }
    inline const uint32_t& get_request_id() const { return request_id_.get(); }
    inline uint32_t request_id() const { return request_id_.get(); }

    static constexpr char const* STATUS_NAME = "status";
    inline void clear_status() { status_.clear(); }
    inline void set_status(const Status& value) { status_ = value; }
    inline void set_status(const Status&& value) { status_ = value; }
    inline const Status& get_status() const { return status_.get(); }
    inline Status status() const { return status_.get(); }

    static constexpr char const* REVISION_NAME = "revision";
    inline void clear_revision() { revision_.clear(); }
    inline void set_revision(const uint32_t& value) { revision_ = value; }
    inline void set_revision(const uint32_t&& value) { revision_ = value; }
    inline uint32_t& mutable_revision() { return revision_.get(); }
    inline const uint32_t& get_revision() const { return revision_.get(); }
    inline uint32_t revision() const { return revision_.get(); }

    static constexpr char const* PROVISIONED_NAME = "provisioned";
    inline void clear_provisioned() { provisioned_.clear(); }
    inline void set_provisioned(const bool& value) { provisioned_ = value; }
    inline void set_provisioned(const bool&& value) { provisioned_ = value; }
    inline bool& mutable_provisioned() { return provisioned_.get(); }
    inline const bool& get_provisioned() const { return provisioned_.get(); }
    inline bool provisioned() const { return provisioned_.get(); }

    static constexpr char const* FIELD_NAME = "field";
    inline void clear_field() { field_.clear(); }
    inline void set_field(const FieldSpec<RemoteReply_field_FieldSpec_name_LENGTH, RemoteReply_field_FieldSpec_group_LENGTH, RemoteReply_field_FieldSpec_source_LENGTH>& value) { field_ = value; }
    inline void set_field(const FieldSpec<RemoteReply_field_FieldSpec_name_LENGTH, RemoteReply_field_FieldSpec_group_LENGTH, RemoteReply_field_FieldSpec_source_LENGTH>&& value) { field_ = value; }
    inline FieldSpec<RemoteReply_field_FieldSpec_name_LENGTH, RemoteReply_field_FieldSpec_group_LENGTH, RemoteReply_field_FieldSpec_source_LENGTH>& mutable_field() { return field_; }
    inline const FieldSpec<RemoteReply_field_FieldSpec_name_LENGTH, RemoteReply_field_FieldSpec_group_LENGTH, RemoteReply_field_FieldSpec_source_LENGTH>& get_field() const { return field_; }
    inline const FieldSpec<RemoteReply_field_FieldSpec_name_LENGTH, RemoteReply_field_FieldSpec_group_LENGTH, RemoteReply_field_FieldSpec_source_LENGTH>& field() const { return field_; }

    static constexpr char const* VALUE_NAME = "value";
    inline void clear_value() { value_.clear(); }
    inline void set_value(const float& value) { value_ = value; }
    inline void set_value(const float&& value) { value_ = value; }
    inline float& mutable_value() { return value_.get(); }
    inline const float& get_value() const { return value_.get(); }
    inline float value() const { return value_.get(); }

    static constexpr char const* SCHEMA_CHUNK_NAME = "schema_chunk";
    inline void clear_schema_chunk() { schema_chunk_.clear(); }
    inline ::EmbeddedProto::FieldBytes<RemoteReply_schema_chunk_LENGTH>& mutable_schema_chunk() { return schema_chunk_; }
    inline void set_schema_chunk(const ::EmbeddedProto::FieldBytes<RemoteReply_schema_chunk_LENGTH>& rhs) { schema_chunk_.set(rhs); }
    inline const ::EmbeddedProto::FieldBytes<RemoteReply_schema_chunk_LENGTH>& get_schema_chunk() const { return schema_chunk_; }
    inline const uint8_t* schema_chunk() const { return schema_chunk_.get_const(); }

    static constexpr char const* TOTAL_NAME = "total";
    inline void clear_total() { total_.clear(); }
    inline void set_total(const uint32_t& value) { total_ = value; }
    inline void set_total(const uint32_t&& value) { total_ = value; }
    inline uint32_t& mutable_total() { return total_.get(); }
    inline const uint32_t& get_total() const { return total_.get(); }
    inline uint32_t total() const { return total_.get(); }

    static constexpr char const* OFFSET_NAME = "offset";
    inline void clear_offset() { offset_.clear(); }
    inline void set_offset(const uint32_t& value) { offset_ = value; }
    inline void set_offset(const uint32_t&& value) { offset_ = value; }
    inline uint32_t& mutable_offset() { return offset_.get(); }
    inline const uint32_t& get_offset() const { return offset_.get(); }
    inline uint32_t offset() const { return offset_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if((static_cast<Status>(0) != status_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = status_.serialize_with_id(static_cast<uint32_t>(FieldNumber::STATUS), buffer, false);
      }

      if((0U != revision_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = revision_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REVISION), buffer, false);
      }

      if((false != provisioned_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = provisioned_.serialize_with_id(static_cast<uint32_t>(FieldNumber::PROVISIONED), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = field_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FIELD), buffer, false);
      }

      if((0.0f != value_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = value_.serialize_with_id(static_cast<uint32_t>(FieldNumber::VALUE), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = schema_chunk_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SCHEMA_CHUNK), buffer, false);
      }

      if((0U != total_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = total_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TOTAL), buffer, false);
      }

      if((0U != offset_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = offset_.serialize_with_id(static_cast<uint32_t>(FieldNumber::OFFSET), buffer, false);
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

          case FieldNumber::STATUS:
            return_value = status_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::REVISION:
            return_value = revision_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::PROVISIONED:
            return_value = provisioned_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FIELD:
            return_value = field_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::VALUE:
            return_value = value_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::SCHEMA_CHUNK:
            return_value = schema_chunk_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::TOTAL:
            return_value = total_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::OFFSET:
            return_value = offset_.deserialize_check_type(buffer, wire_type);
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
      clear_status();
      clear_revision();
      clear_provisioned();
      clear_field();
      clear_value();
      clear_schema_chunk();
      clear_total();
      clear_offset();

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
        case FieldNumber::STATUS:
          name = STATUS_NAME;
          break;
        case FieldNumber::REVISION:
          name = REVISION_NAME;
          break;
        case FieldNumber::PROVISIONED:
          name = PROVISIONED_NAME;
          break;
        case FieldNumber::FIELD:
          name = FIELD_NAME;
          break;
        case FieldNumber::VALUE:
          name = VALUE_NAME;
          break;
        case FieldNumber::SCHEMA_CHUNK:
          name = SCHEMA_CHUNK_NAME;
          break;
        case FieldNumber::TOTAL:
          name = TOTAL_NAME;
          break;
        case FieldNumber::OFFSET:
          name = OFFSET_NAME;
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
          + EmbeddedProto::enumeration<Status, EmbeddedProto::WireFormatter::VarintSize(6)>::max_serialized_size(static_cast<uint32_t>(FieldNumber::STATUS))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::REVISION))
          + EmbeddedProto::boolean::max_serialized_size(static_cast<uint32_t>(FieldNumber::PROVISIONED))
          + FieldSpec<RemoteReply_field_FieldSpec_name_LENGTH, RemoteReply_field_FieldSpec_group_LENGTH, RemoteReply_field_FieldSpec_source_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::FIELD))
          + EmbeddedProto::floatfixed::max_serialized_size(static_cast<uint32_t>(FieldNumber::VALUE))
          + ::EmbeddedProto::FieldBytes<RemoteReply_schema_chunk_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::SCHEMA_CHUNK))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::TOTAL))
          + EmbeddedProto::uint32::max_serialized_size(static_cast<uint32_t>(FieldNumber::OFFSET))
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
      left_chars = status_.to_string(left_chars, indent_level + 2, STATUS_NAME, false);
      left_chars = revision_.to_string(left_chars, indent_level + 2, REVISION_NAME, false);
      left_chars = provisioned_.to_string(left_chars, indent_level + 2, PROVISIONED_NAME, false);
      left_chars = field_.to_string(left_chars, indent_level + 2, FIELD_NAME, false);
      left_chars = value_.to_string(left_chars, indent_level + 2, VALUE_NAME, false);
      left_chars = schema_chunk_.to_string(left_chars, indent_level + 2, SCHEMA_CHUNK_NAME, false);
      left_chars = total_.to_string(left_chars, indent_level + 2, TOTAL_NAME, false);
      left_chars = offset_.to_string(left_chars, indent_level + 2, OFFSET_NAME, false);
  
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
      EmbeddedProto::enumeration<Status, EmbeddedProto::WireFormatter::VarintSize(6)> status_ = static_cast<Status>(0);
      EmbeddedProto::uint32 revision_ = 0U;
      EmbeddedProto::boolean provisioned_ = false;
      FieldSpec<RemoteReply_field_FieldSpec_name_LENGTH, RemoteReply_field_FieldSpec_group_LENGTH, RemoteReply_field_FieldSpec_source_LENGTH> field_;
      EmbeddedProto::floatfixed value_ = 0.0f;
      ::EmbeddedProto::FieldBytes<RemoteReply_schema_chunk_LENGTH> schema_chunk_;
      EmbeddedProto::uint32 total_ = 0U;
      EmbeddedProto::uint32 offset_ = 0U;

};

} // End of namespace v1
} // End of namespace remote
} // End of namespace axi
#endif // REMOTE_CONFIG_H