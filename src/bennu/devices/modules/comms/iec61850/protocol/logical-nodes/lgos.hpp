/**
   @brief This file will describe the definition of a IEC61850 GOOSE subscription logical node.
*/
#ifndef __IEC61850_LGOS__
#define __IEC61850_LGOS__

/* stl includes */
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/goose/data-set.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/logical-node.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::map;
    using std::string;

    /**
       @brief This class defines the LGOS logical node as described in
       IEC61850-7-4 (5.16.2)
     */
    class lgos : public logical_node {
    public:
      lgos( string name, data_set ds );

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

    private:
      string name_;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_LGOS__ */
