/* stl includes */
#include <functional>

#include "bennu/devices/modules/comms/iec61850/device/attribute-map.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"

namespace ccss_devices {
  namespace iec61850 {
    namespace goose {

      using std::placeholders::_1;
      using namespace ccss_protocols::iec61850;

      attribute_map::attribute_map() {
        attr_map.insert
          ( std::pair<string, add_attribute_fn_t>
            ("Boolean", std::bind
             (static_cast<void(data_set::*)(void)>(&data_set::add_attribute<Boolean>), _1)));

        // SEL lists their bool type as "BOOLEAN" in the CID file
        attr_map.insert
          ( std::pair<string, add_attribute_fn_t>
            ("BOOLEAN", std::bind
             (static_cast<void(data_set::*)(void)>(&data_set::add_attribute<Boolean>), _1)));

        attr_map.insert
          ( std::pair<string, add_attribute_fn_t>
            ("INT32U", std::bind
             (static_cast<void(data_set::*)(void)>(&data_set::add_attribute<INT32U>), _1)));

        attr_map.insert
          ( std::pair<string, add_attribute_fn_t>
            ("INT32", std::bind
             (static_cast<void(data_set::*)(void)>(&data_set::add_attribute<INT32>), _1)));

        attr_map.insert
          ( std::pair<string, add_attribute_fn_t>
            ("FLOAT32", std::bind
             (static_cast<void(data_set::*)(void)>(&data_set::add_attribute<FLOAT32>), _1)));

        attr_map.insert
          ( std::pair<string, add_attribute_fn_t>
            ("VISIBLE_STRING", std::bind
             (static_cast<void(data_set::*)(void)>(&data_set::add_attribute<VISIBLE_STRING>), _1)));

        // SEL lists their bool type as "VisString255" in the CID file
        attr_map.insert
          ( std::pair<string, add_attribute_fn_t>
            ("VisString255", std::bind
             (static_cast<void(data_set::*)(void)>(&data_set::add_attribute<VISIBLE_STRING>), _1)));

        attr_map.insert
          ( std::pair<string, add_attribute_fn_t>
            ("Timestamp", std::bind
             (static_cast<void(data_set::*)(void)>(&data_set::add_attribute<UtcTime>), _1)));
      }

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_devices
