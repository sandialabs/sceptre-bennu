/**
   @brief This file implements the definition of an IEC61850 Circuit Breaker logical node
   found in xcbr.hpp
 */
#include "bennu/devices/modules/comms/iec61850/protocol/common-data-classes/dpc.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/common-data-classes/ins.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/common-data-classes/spc.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/common-data-classes/sps.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/logical-nodes/xcbr.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::pair;
    using std::make_pair;

    xcbr::xcbr( string name ) : name_(name) {
      sps loc("Loc");
      data_objects.insert( pair<string, data_object>(loc.get_name(), loc) );
      ins opcnt("OpCnt");
      data_objects.insert( pair<string, data_object>(opcnt.get_name(), opcnt) );
      dpc pos("Pos");
      data_objects.insert( pair<string, data_object>(pos.get_name(), pos) );
      spc blkopn("BlkOpn");
      data_objects.insert( pair<string, data_object>(blkopn.get_name(), blkopn) );
      spc blkcls("BlkCls");
      data_objects.insert( pair<string, data_object>(blkcls.get_name(), blkcls) );
      ins cbopcap("CBOpCap");
      data_objects.insert( pair<string, data_object>(blkcls.get_name(), cbopcap) );
    }

  } // namespace iec61850

} // namespace ccss_protocols
