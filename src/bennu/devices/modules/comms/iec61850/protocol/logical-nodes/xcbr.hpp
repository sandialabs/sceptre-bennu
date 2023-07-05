/**
   @brief This file will describe the definition of a IEC61850 Circuit Breaker logical node.
*/
#ifndef __IEC61850_XCBR__
#define __IEC61850_XCBR__

#include "bennu/devices/modules/comms/iec61850/protocol/logical-node.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::map;

    /**
       @brief This class defines the XCBR logical node as described in
       IEC61850-7-4 (5.16.2)
     */
    class xcbr : public logical_node {
    public:
      xcbr() {}
      xcbr( string name );

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

    private:
      string name_;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_XCBR__ */
