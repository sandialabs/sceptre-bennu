/**
   @brief This file will describe the definition of a IEC61850 logical node.
*/
#ifndef __IEC61850_LLN0__
#define __IEC61850_LLN0__

/* stl includes */
#include <vector>

#include "bennu/devices/modules/comms/iec61850/protocol/logical-node.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::vector;

    /**
       @brief This class defines the LLN0 logical node as described in
       IEC61850-7-420 (Table 4)

       NOTE: All fields of this node are optional, therefore, for the time being we are choosing
       to leave them out.
     */
    class lln0 : public logical_node {
    public:
      lln0() {}

      map<string, gocb> goose_cbs;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_LLN0__ */
