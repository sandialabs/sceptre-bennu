/**
   @brief This file implements the definition of an IEC61850 logical device
   found in logical-device.hpp
 */
/* stl includes */
#include <map>
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/logical-device.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/logical-node.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::pair;
    using std::make_pair;
    using std::map;
    using std::string;

    logical_device::logical_device( string name, logical_node lln0 ) : name_(name) {
      logical_nodes.insert( pair<string, logical_node>(lln0.get_name(), lln0) );
    }

    /* Services */
    map<string, logical_node> logical_device::get_logical_device_dir( void ) {
      return logical_nodes;
    }

  } // namespace iec61850

} // namespace ccss_protocols
