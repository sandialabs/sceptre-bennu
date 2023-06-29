/**
   @brief This file describes the native format for a GOOSE protocol header
*/
#ifndef __IEC61850_GOOSE_HEADER_HPP__
#define __IEC61850_GOOSE_HEADER_HPP__

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types-traits.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types-serialization.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"

namespace ccss_protocols { namespace iec61850 { namespace goose {

      /**< Tag value for a GOOSE PDU */
      const uint8_t GOOSE_HEADER_TAG = 0x61;

      struct goCBRef_t {
        typedef VISIBLE_STRING value_type;
        enum { tag_value = 0x80 };
      };

      struct timeAllowedToLive_t {
        typedef INT32U value_type;
        enum { tag_value = 0x81 };
      };

      struct datSet_t {
        typedef VISIBLE_STRING value_type;
        enum { tag_value = 0x82 };
        enum { length_value = 0x00 };
      };

      struct goID_t {
        typedef VISIBLE_STRING value_type;
        enum { tag_value = 0x83 };
      };

      struct T_t {
        typedef UtcTime value_type;
        enum { tag_value = 0x84 };
      };

      struct stNum_t {
        typedef INT32U value_type;
        enum { tag_value = 0x85 };
      };

      struct sqNum_t {
        typedef INT32U value_type;
        enum { tag_value = 0x86 };
      };

      struct simulation_t {
        typedef Boolean value_type;
        enum { tag_value = 0x87 };
      };

      struct confRev_t {
        typedef INT32U value_type;
        enum { tag_value = 0x88 };
      };

      struct ndsCom_t {
        typedef Boolean value_type;
        enum { tag_value = 0x89 };
      };

      struct numDatSetEntries_t {
        typedef INT32U value_type;
        enum { tag_value = 0x8a };
      };

      /**@brief GOOSE Header field defined as a (Tag, Length, Value) triplet
         specified as per IEC61850-8-1 (Table A.1)

         These triplets make up the fields in the GOOSE Header
      */
      template<typename FIELD_T>
      struct header_field_t {
        header_field_t() : tag(FIELD_T::tag_value),
                           length(FIELD_T::value_type::fixed_length) {}


        const uint8_t tag;
        uint8_t length;
        typename FIELD_T::value_type::value_type value;
      };

      /**@brief GOOSE Header

         Defined as a collection of GOOSE header fields. This header is what the GOOSE
         message data looks like on the wire.  The GOOSE message state comes from a
         goose_msg object.
      */
      struct header_t {
        header_field_t<goCBRef_t> goCBRef;
        header_field_t<timeAllowedToLive_t> timeAllowedToLive;
        header_field_t<datSet_t> datSet;
        header_field_t<goID_t> goID;
        header_field_t<T_t> T;
        header_field_t<stNum_t> stNum;
        header_field_t<sqNum_t> sqNum;
        header_field_t<simulation_t> simulation;
        header_field_t<confRev_t> confRev;
        header_field_t<ndsCom_t> ndsCom;
        header_field_t<numDatSetEntries_t> numDatSetEntries;
      };

      namespace header {

        /**@brief This function calculates the length of the populated header.

           @param hdr goose::header_t structure to be serialized

           @param with_meta specifies whether or not to return the length with
           the tag and length fields counted for each field.

           @return size of the goose::header_t
         */
        size_t calculate_length( header_t const& hdr, bool with_meta = true );

        /**@brief This function takes a header structure and writes it out
           to a serialized buffer as it would be seen on the wire

           This function assumes a continuous buffer.  If you are using
           a vector to serialize data into, make sure you call resize() on the
           vector before calling this function on a goose::header_t.

           @param hdr goose::header_t structure to be serialized

           @param write pointer into the buffer to copy in the GOOSE header data

           @param last pointer to one byte past the end of the writable buffer

           @return pointer to the next valid writable location in the
           writable buffer
         */
        uint8_t* serialize( header_t hdr, uint8_t* write, uint8_t const* last );

        namespace field {

          /**@brief This function takes a templated GOOSE header field from a
             headert_t instance and sets the field value of that field.

             The purpose of providing a function to perform the write is to
             ensure that the length field is always set for fields.  This is
             necessary when serializing data for the wire.  Specializations
             for the header fields with a value_type of VISIBLE_STRING is
             necessary because their length field is not filled in at time of
             instantiation.  It must be set because that type is represented
             with a std::string internally and thus has a variable width

             @param hfield templatized header field whose value is to be set

             @param value value to write to hfield
           */
          template<typename FIELD_T>
          void set( header_field_t<FIELD_T>& hfield, typename FIELD_T::value_type::value_type value ) {
            hfield.length = iec_basic_type_traits<typename FIELD_T::value_type>::size(value);
            hfield.value = value;
          }

          /**@brief This function takes a templated (and populated) GOOSE header
             field from a header_t instance and writes the data into a flat buffer
             for transimission on the wire

             @param hfield templatized header field from a header_t struc to be serialized

             @param write  pointer into the flat buffer to serialize the hfield
             parameter into. This pointer will be updated with the new write position
             when the function returns

             @param last   pointer to one byte past the end of your buffer.

             @return next valid write location in the buffer
          */
          template<typename FIELD_T>
          uint8_t* serialize
          ( header_field_t<FIELD_T> hfield, uint8_t* write, uint8_t const* last ) {
            uint8_t* new_write = write;

            // total serialized size is the length of the tag + length + value fields
            size_t field_size = hfield.length + 2;

            if ( field_size >= (last - write) ) {
              // the size of this field exceeds the size of the user buffer
              throw ccss_protocols::Exception
                ("Insufficient buffer space to serialize GOOSE header field.");
            }

            // serialize the tag
            *new_write = hfield.tag;
            new_write++;

            // serialize the length
            *new_write = hfield.length;
            new_write++;

            // serialize the value
            new_write =
              iec61850::basictype::serialize
              <typename FIELD_T::value_type>
              (hfield.value, new_write, last, false);

            return new_write;
          }
        } // namespace field

      } // namespace header

    }  /*namespace goose*/ } /*namespace iec61850*/ } /*namespace ccss_protocols*/

#endif /* __IEC61850_GOOSE_HEADER_HPP__ */
