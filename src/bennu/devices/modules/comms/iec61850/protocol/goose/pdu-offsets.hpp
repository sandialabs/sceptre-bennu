/**
   @brief This file defines offsets into the GOOSE PDU
*/
#ifndef __IEC61850_GOOSE_PDU_OFFSETS__
#define __IEC61850_GOOSE_PDU_OFFSETS__

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {

      const uint8_t PREAMBLE_OFFSET = 0x00;
      const uint8_t GOOSE_MESSAGE_TAG_OFFSET = 0x08;

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_protocols


#endif /*  __IEC61850_GOOSE_PDU_OFFSETS__ */
