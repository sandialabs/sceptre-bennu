/**
   @brief Defines a type attribute mapping
*/
#ifndef __IEC61850_GOOSE_ATTRIBUTE_MAP_HPP__
#define __IEC61850_GOOSE_ATTRIBUTE_MAP_HPP__

/* stl includes */
#include <map>
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/goose/data-set.hpp"

namespace ccss_devices {
  namespace iec61850 {
    namespace goose {

      using std::map;
      using std::string;

      using ccss_protocols::iec61850::goose::data_set;

      typedef std::function<void (data_set*)> add_attribute_fn_t;

      class attribute_map {
      public:
        attribute_map();
        map<string, add_attribute_fn_t> attr_map;
      };

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_devices

#endif /* __IEC61850_GOOSE_ATTRIBUTE_MAP_HPP__ */
