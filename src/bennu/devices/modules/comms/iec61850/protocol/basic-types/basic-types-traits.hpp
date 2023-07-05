/**
   @brief This file describes a model to retrive iec61850::basic-type info
   based on a c++ type
*/
#ifndef __IEC61850_BASIC_TYPE_TRAITS_HPP__
#define __IEC61850_BASIC_TYPE_TRAITS_HPP__

/* stl includes */
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    /**
       Type traits specifying certain attributes of the IEC Basic types
    */
    template <typename IEC_BASIC_T>
    struct iec_basic_type_traits {
      static size_t size(typename IEC_BASIC_T::value_type const& /*v*/)
      { return IEC_BASIC_T::fixed_length; }

      static typename IEC_BASIC_T::value_type get(basic_value_t const& bv)
      { return *reinterpret_cast<typename IEC_BASIC_T::value_type const*>(&bv.val[0]); }

      static void set(basic_value_t& bv, typename IEC_BASIC_T::value_type const& v)
      { *reinterpret_cast<typename IEC_BASIC_T::value_type*>(&bv.val[0]) = v; }
    };

    template <>
    struct iec_basic_type_traits<VISIBLE_STRING> {
      static size_t size(typename VISIBLE_STRING::value_type const& v)
      { return v.size(); }

      static typename VISIBLE_STRING::value_type get(basic_value_t const& bv)
      { return VISIBLE_STRING::value_type(bv.val.begin(), bv.val.end()); }

      static void set(basic_value_t& bv, typename VISIBLE_STRING::value_type const& v)
      { bv.val.resize(v.size()); std::copy(v.begin(), v.end(), &bv.val[0]); }
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /*__IEC61850_BASIC_TYPE_TRAITS_HPP__*/
