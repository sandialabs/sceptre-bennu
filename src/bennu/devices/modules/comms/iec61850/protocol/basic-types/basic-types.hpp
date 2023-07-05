/**
   @brief basic-types.hpp defines the necessary data structures for IEC61850-8-1
   (Table A.2) GOOSE Basic-Types
*/
#ifndef __IEC61850_BASIC_TYPES_HPP__
#define __IEC61850_BASIC_TYPES_HPP__

/* stl includes */
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::map;
    using std::string;
    using std::vector;
    using std::uint8_t;
    using std::uint32_t;
    using std::make_pair;

    /**
        status_t and model_t represent types as used in Common Data Classes
        IEC61850-7-3
    */
    struct status_t {
      enum value {
        INTERMEDIATE,
        OFF,
        ON,
        BAD
      };
    };

    struct model_t {
      enum value {
        STATUS_ONLY,
        DIRECT_WITH_NORM_SEC,
        SBO_WITH_NORM_SEC,
        DIRECT_WITH_ENHANCED_SEC,
        SBO_WITH_ENHANCED_SEC
      };
    };

    std::map<std::uint8_t, std::string>& basic_type_registry_singleton();

    /**
        The REGISTER_TYPEMAP() MACRO uses the c++11 feature "user-defined literals"
        alongside with the preprocessor to populate a run-time data-structure that
        maps b/w IEC_BASIC_T::tag_value's and the stringified IEC_BASIC_T.

        The TYPEMAP() MACRO provides a simple facility for generating a typemapping
        from internal C++-types to IEC61850 Basic types.
    */
#ifdef TYPEMAP_INSTANTIATION_SINGLETON
#define REGISTER_TYPEMAP(IEC_BASIC_T) \
    inline int operator "" _##IEC_BASIC_T(unsigned long long)                         \
    {                                                                                 \
      basic_type_registry_singleton()[IEC_BASIC_T::tag_value] = string(#IEC_BASIC_T); \
      return (0);                                                                     \
    }                                                                                 \
    int const _unused_##IEC_BASIC_T = 0_##IEC_BASIC_T
#else
#define REGISTER_TYPEMAP(IEC_BASIC_T)
#endif

    template <typename T>
    struct iec_basic_type;

#define TYPEMAP(IEC_BASIC_T)                                                          \
    template<>                                                                        \
    struct iec_basic_type<IEC_BASIC_T::value_type> {                                  \
      typedef IEC_BASIC_T type;                                                       \
    };                                                                                \
    REGISTER_TYPEMAP(IEC_BASIC_T)

    /**
       The value_t struct allows for data with varying types to be stored in a
       common format within the context of another container.

       NOTE: This will usually be a data attribute for this protocol
    */
    struct basic_value_t {
      uint8_t type;        /**<Must evaluate to a basic type (asn.1) as defined below*/
      vector<uint8_t> val; /**<Container for the actual value stored*/
    };

    /**
       The following types represent basic types as specified in IEC61850
    */
    struct UTC_TIME_T {
      uint8_t t0;
      uint8_t t1;
      uint8_t t2;
      uint8_t t3;
      uint8_t t4;
      uint8_t t5;
      uint8_t t6;
      uint8_t t7;
    };

    /**
       These types represent the types defined in the IEC61850 specification.

       The tag_value is the ASN.1 tag as specified by the specification.  The
       fixed length is the fixed length as specified in the standard.  The
       value_type is c++ representation of that type.  Some types, such as
       VISIBLE_STRING, may have variable width lengths.  Their fixed_length
       will be defined as 0.  The required the c++ type representing that
       IEC Basic type to have a size() method.
    */
    struct Boolean {
      enum { tag_value = 0x83 };
      enum { fixed_length = 1 };
      typedef bool value_type;
    }; TYPEMAP(Boolean);

    struct UtcTime {
      enum { tag_value = 0x84 };
      enum { fixed_length = 8 };
      typedef UTC_TIME_T value_type;
    }; TYPEMAP(UtcTime);

    struct INT32 {
      enum { tag_value = 0x85 };
      enum { fixed_length = 5 };
      typedef int32_t value_type;
    }; TYPEMAP(INT32);

    struct INT32U {
      enum { tag_value = 0x86 };
      enum { fixed_length = 5 };
      typedef uint32_t value_type;
    }; TYPEMAP(INT32U);

    struct FLOAT32 {
      enum { tag_value = 0x87 };
      enum { fixed_length = 4 };
      typedef float value_type;
    }; TYPEMAP(FLOAT32);

    struct VISIBLE_STRING {
      enum { tag_value = 0x8a };
      enum { fixed_length = 0x00 }; // this length is always variable
      typedef string value_type;
    }; TYPEMAP(VISIBLE_STRING);

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_BASIC_TYPES_HPP__ */
