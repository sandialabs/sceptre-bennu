/**
   @brief Defines the serialization routines for basic-types
*/
#ifndef __IEC61850_BASIC_TYPE_SERIALIZATION_HPP__
#define __IEC61850_BASIC_TYPE_SERIALIZATION_HPP__

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/copy-policies.hpp"

namespace detail {
  template<typename IEC_BASIC_T>
  inline void serialize_tag_length(uint8_t* write, uint8_t const* last, bool serialize_meta) {
    uint8_t* new_write = write;
    size_t type_size = IEC_BASIC_T::fixed_length;

    if (serialize_meta) {
      type_size += 2; // add the tag + length field to the type_size

      if ( type_size > (last - write) ) {
        // the size of this field exceeds the size of the user buffer
        throw ccss_protocols::Exception
          ("Insufficient buffer space to serialize IEC61850 basic type.");
      }

      // serialize the tag
      *new_write = IEC_BASIC_T::tag_value;
      new_write++;

      // serialize the length
      *new_write = IEC_BASIC_T::fixed_length;
      new_write++;
    }
    else {
      if ( type_size > (last - write) ) {
        // the size of this field exceeds the size of the user buffer
        throw ccss_protocols::Exception
          ("Insufficient buffer space to serialize IEC61850 basic type.");
      }
    }
  }
} // namespace detail

namespace ccss_protocols {

  namespace iec61850 {
    namespace basictype {
      /** @brief Generic templatized implementation of serialize() for IEC61850
          basictypes

          NOTE: This function throws if there is not enough space in the provided
          buffer to serialize the type.

          @param value Value to serialize

          @param write Position into a flat buffer to begin writing the type into

          @param last One byte past the end of the buffer to write into

          @param serialize_meta This is a flag that indicates whether or not to
          write the tag and length values of the basic type into the provided
          buffer.  By default the answer is yes.  However, in the case of the
          GOOSE header fields, this is undesireable.

          @return Next valid write position into the buffer
      */
      template<typename IEC_BASIC_T, typename COPY_POLICY = cpolicy::nbo_copy>
      uint8_t* serialize
      (typename IEC_BASIC_T::value_type value, uint8_t* write, uint8_t const* last, bool serialize_meta=true) {
        uint8_t* new_write = write;

        // serialize the tag and length (if applicable)
        detail::serialize_tag_length<IEC_BASIC_T>( new_write, last, serialize_meta );

        // copy the value in network byte order (nbo)
        new_write = COPY_POLICY::apply(value, new_write);

        return new_write;
      }
    } // namespace basictype
  } // namespace iec61850

} // namespace ccss_protocols

#endif /*__IEC61850_BASIC_TYPE_SERIALIZATION_HPP__*/
