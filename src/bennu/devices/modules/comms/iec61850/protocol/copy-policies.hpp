/**
   @brief This file defines a number of policy classes for copying data.

   The initial two policies implemented in this file are fcopy and
   rcopy.  They are intended to handle copying numeric data in nbo
   and string data as is.  They were implemented as policies because one
   could certainly imagine needing a more specialized version of a copy
   routine.  In addtion it simplifies code in other files that use the
   policies (e.g. basic-types.hpp)

   All copy-policies are templated on the IEC_BASIC_T types defined in
   basic-types.hpp.  The 'loc' (location) pointer is an in-out parameter.
   It will tell the policy function where to initially copy the data, but
   will then be updated with the new write location.
*/
#ifndef __IEC61850_COPY_POLICIES_HPP__
#define __IEC61850_COPY_POLICIES_HPP__

/* stl includes */
#include <algorithm>
#include <cstdint>

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types-traits.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::uint8_t;

    namespace cpolicy {

      /**@brief This policy copies data in network byte order into a buffer, and
         returns the next location in the buffer that is writable*/
      class nbo_copy {
      public:
        template <typename T>
        static uint8_t* apply(T const& value, uint8_t* loc) {
          uint8_t const* start = reinterpret_cast<uint8_t const*>(&value);
          std::reverse_copy(start,
                            start + iec_basic_type<T>::type::fixed_length, loc);
          loc += iec_basic_type<T>::type::fixed_length;

          return loc;
        }
      };

      template <>
      inline uint8_t* nbo_copy::apply<INT32U::value_type>
      (INT32U::value_type const& value, uint8_t* loc) {
        uint8_t const* start = reinterpret_cast<uint8_t const*>(&value);

        /* Need to allow room for an extra byte here.  32-bit integers are
           represented with 5 bytes in GOOSE (go figure).  However, INT32U::value_type
           resolves to a std::uint32_t (only four bytes long). */
        *loc = 0x00;
        loc++;

        std::reverse_copy(start,
                          start + sizeof(typename INT32U::value_type), loc);
        loc += sizeof(typename INT32U::value_type);

        return loc;
      }

      template <>
      inline uint8_t* nbo_copy::apply<VISIBLE_STRING::value_type>
      (VISIBLE_STRING::value_type const& value, uint8_t* loc) {
        std::copy(value.data(), value.data() + value.size(), loc);
        loc += value.size();

        return loc;
      }

      /**@brief This policy copies data in host byte order into a buffer,
         and returns the next location in the buffer that is writeable*/
      class hbo_copy {
      public:
        template <typename T>
        static uint8_t* apply(T const& value, uint8_t* loc) {
          *reinterpret_cast<T*>(loc) = value;
          loc += sizeof(T);

          return loc;
        }
      };

      template <>
      inline uint8_t* hbo_copy::apply<VISIBLE_STRING::value_type>
      (typename VISIBLE_STRING::value_type const& value, uint8_t* loc) {
        std::copy(value.data(), value.data() + value.size(), loc);
        loc += value.size();

        return loc;
      }

    }  // namespace cpolicy

  } // namespace iec61850

} // namespace ccss_protocols

#endif /*__IEC61850_COPY_POLICIES_HPP__*/
