/**
   @brief This file will describe the definition of an IEC61850 logical device.
*/
#ifndef __IEC61850_LOGICAL_DEVICE__
#define __IEC61850_LOGICAL_DEVICE__

/* stl includes */
#include <map>
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/common-acsi-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/logical-node.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::map;
    using std::string;

    /**
       @brief This class defines the GenLogicalDeviceClass as described in
       IEC61850-7-2 (Sec. 9.1.1 Table 15)
    */
    class logical_device {
    public:
      logical_device() {}
      logical_device( string name ) : name_(name) {}
      logical_device( string name, logical_node lln0 );

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

      /* Services */
      map<string, logical_node> get_logical_device_dir( void );

      /* container of logical nodes (represnting this device's internal data-store */
      map<string, logical_node> logical_nodes;

      /* container of logical nodes (represnting this device's subscriptions to other
     devices GOOSE control blocks */
      map<string, logical_node> subscription_logical_nodes;

    private:
      string name_;
    };

    // this is the highest level data structure for the IEC61850(GOOSE) data-store
    typedef vector<logical_device> server;

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_LOGICAL_DEVICE__ */
