#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/header.hpp"

namespace ccss_protocols { namespace iec61850 { namespace goose {

      namespace header {

        size_t calculate_length( header_t const& hdr, bool with_meta ) {
          // 11 fields * (1 tag + 1 length)
          const uint8_t HEADER_METADATA_SIZE = (11 *2);

          size_t hdr_length = 0;
          if (with_meta) {
            hdr_length = HEADER_METADATA_SIZE;
          }

          hdr_length +=
            hdr.goCBRef.length +
            hdr.timeAllowedToLive.length +
            hdr.datSet.length +
            hdr.goID.length +
            hdr.T.length +
            hdr.stNum.length +
            hdr.sqNum.length +
            hdr.simulation.length +
            hdr.confRev.length +
            hdr.ndsCom.length +
            hdr.numDatSetEntries.length;

          return hdr_length;
        }

        uint8_t* serialize( header_t hdr, uint8_t* write, uint8_t const* last ) {
          uint8_t* new_write = write;

          // serialize the GOOSE header fields
          new_write = header::field::serialize(hdr.goCBRef, new_write, last);
          new_write = header::field::serialize(hdr.timeAllowedToLive, new_write, last);
          new_write = header::field::serialize(hdr.datSet, new_write, last);
          new_write = header::field::serialize(hdr.goID, new_write, last);
          new_write = header::field::serialize(hdr.T, new_write, last);
          new_write = header::field::serialize(hdr.stNum, new_write, last);
          new_write = header::field::serialize(hdr.sqNum, new_write, last);
          new_write = header::field::serialize(hdr.simulation, new_write, last);
          new_write = header::field::serialize(hdr.confRev, new_write, last);
          new_write = header::field::serialize(hdr.ndsCom, new_write, last);
          new_write = header::field::serialize(hdr.numDatSetEntries, new_write, last);

          return new_write;
        }

      } // namespace header

    }  /*namespace goose*/ } /*namespace iec61850*/ } /*namespace ccss_protocols*/
