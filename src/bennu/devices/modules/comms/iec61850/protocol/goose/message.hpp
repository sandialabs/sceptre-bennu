/**
   @brief This file contains the definition of a GOOSE Message
   as defined in IEC61850-7-2 Section 18.2.3.1
*/
#ifndef __IEC61850_GOOSE_MESSAGE_DEFINITION_HPP__
#define __IEC61850_GOOSE_MESSAGE_DEFINITION_HPP__

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {
      struct goose_msg {
        typename VISIBLE_STRING::value_type DatSet;
        typename VISIBLE_STRING::value_type GoID;
        typename VISIBLE_STRING::value_type GoCBRef;
        typename UtcTime::value_type T;
        typename INT32U::value_type StNum;
        typename INT32U::value_type SqNum;
        typename Boolean::value_type Simulation;
        typename INT32U::value_type ConfRev;
        typename Boolean::value_type NdsCom;
      };
    } // namespace goose
  } // namespace iec61850
} // namespace ccss_protocols

#endif /* __IEC61850_GOOSE_MESSAGE_DEFINITION_HPP__ */
