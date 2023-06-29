/**
   @brief This file describes the definition of a IEC61850 logical node.
*/
#ifndef __IEC61850_LOGICAL_NODE__
#define __IEC61850_LOGICAL_NODE__

/* stl includes */
#include <map>
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/common-acsi-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/data-attribute.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/data-object.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/data-set.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/gocb.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::map;
    using std::string;

    using goose::data_set;

    /**
       @brief This class defines the GenLogicalNodeClass as described in
       IEC61850-7-2-2 (Table 16)
     */
    class logical_node {

    public:
      logical_node() {}
      logical_node( string name ) : ln_name_(name) {}

      void set_name( string name ) { ln_name_ = name; }
      string get_name( void ) { return ln_name_; }

      void set_objref( string objref ) { ln_ref_ = objref; }
      string get_objref( void ) { return ln_ref_; }

      map<string, data_set> data_sets;
      map<string, data_object> data_objects;

    private:
      string ln_name_;
      string ln_ref_;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_LOGICAL_NODE__ */
