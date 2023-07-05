/**
   @brief This file implements the definition of an IEC61850 GOOSE subscription logical node
   found in lgos.hpp
 */
/* stl includes */
#include <map>
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/common-data-classes/org.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/common-data-classes/sps.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/logical-nodes/lgos.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::pair;
    using goose::data_set;

    lgos::lgos( string name, data_set ds ) : name_(name) {
      sps st("St"); // Status of the subscription (True if active / False if inactive)
      data_objects.insert( pair<string, data_object>(st.get_name(), st) );

      org gocbref("GoCBRef"); // Reference to the subscribed Goose control block
      data_objects.insert( pair<string, data_object>(gocbref.get_name(), gocbref) );

      data_sets.insert( pair<string, data_set>(ds.name(), ds) );
    }

  } // namespace iec61850

} // namespace ccss_protocols
