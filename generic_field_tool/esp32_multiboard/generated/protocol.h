/*
 *  This file is generated with Embedded Proto, PLEASE DO NOT EDIT!
 *  source: protocol.proto
 */

// This file is generated. Please do not edit!
#ifndef PROTOCOL_H
#define PROTOCOL_H

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

namespace generic {
namespace field {
namespace tool {
namespace v1 {

template<
    uint32_t Envelope_type_LENGTH, 
    uint32_t Envelope_payload_LENGTH
>
class Envelope final: public ::EmbeddedProto::MessageInterface
{
  public:
    Envelope() = default;
    Envelope(const Envelope& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_type(rhs.get_type());
      set_payload(rhs.get_payload());
    }

    Envelope(const Envelope&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_type(rhs.get_type());
      set_payload(rhs.get_payload());
    }

    ~Envelope() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      TYPE = 2,
      PAYLOAD = 3
    };

    Envelope& operator=(const Envelope& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_type(rhs.get_type());
      set_payload(rhs.get_payload());
      return *this;
    }

    Envelope& operator=(const Envelope&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_type(rhs.get_type());
      set_payload(rhs.get_payload());
      return *this;
    }

    static constexpr char const* REQUEST_ID_NAME = "request_id";
    inline void clear_request_id() { request_id_.clear(); }
    inline void set_request_id(const uint32_t& value) { request_id_ = value; }
    inline void set_request_id(const uint32_t&& value) { request_id_ = value; }
    inline uint32_t& mutable_request_id() { return request_id_.get(); }
    inline const uint32_t& get_request_id() const { return request_id_.get(); }
    inline uint32_t request_id() const { return request_id_.get(); }

    static constexpr char const* TYPE_NAME = "type";
    inline void clear_type() { type_.clear(); }
    inline ::EmbeddedProto::FieldString<Envelope_type_LENGTH>& mutable_type() { return type_; }
    inline void set_type(const ::EmbeddedProto::FieldString<Envelope_type_LENGTH>& rhs) { type_.set(rhs); }
    inline const ::EmbeddedProto::FieldString<Envelope_type_LENGTH>& get_type() const { return type_; }
    inline const char* type() const { return type_.get_const(); }

    static constexpr char const* PAYLOAD_NAME = "payload";
    inline void clear_payload() { payload_.clear(); }
    inline ::EmbeddedProto::FieldBytes<Envelope_payload_LENGTH>& mutable_payload() { return payload_; }
    inline void set_payload(const ::EmbeddedProto::FieldBytes<Envelope_payload_LENGTH>& rhs) { payload_.set(rhs); }
    inline const ::EmbeddedProto::FieldBytes<Envelope_payload_LENGTH>& get_payload() const { return payload_; }
    inline const uint8_t* payload() const { return payload_.get_const(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = type_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TYPE), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = payload_.serialize_with_id(static_cast<uint32_t>(FieldNumber::PAYLOAD), buffer, false);
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

          case FieldNumber::TYPE:
            return_value = type_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::PAYLOAD:
            return_value = payload_.deserialize_check_type(buffer, wire_type);
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
      clear_type();
      clear_payload();

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
        case FieldNumber::TYPE:
          name = TYPE_NAME;
          break;
        case FieldNumber::PAYLOAD:
          name = PAYLOAD_NAME;
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
          + ::EmbeddedProto::FieldString<Envelope_type_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::TYPE))
          + ::EmbeddedProto::FieldBytes<Envelope_payload_LENGTH>::max_serialized_size(static_cast<uint32_t>(FieldNumber::PAYLOAD))
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
      left_chars = type_.to_string(left_chars, indent_level + 2, TYPE_NAME, false);
      left_chars = payload_.to_string(left_chars, indent_level + 2, PAYLOAD_NAME, false);
  
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
      ::EmbeddedProto::FieldString<Envelope_type_LENGTH> type_;
      ::EmbeddedProto::FieldBytes<Envelope_payload_LENGTH> payload_;

};

} // End of namespace v1
} // End of namespace tool
} // End of namespace field
} // End of namespace generic
#endif // PROTOCOL_H