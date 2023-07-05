/**
   @brief This file defines the application layer API of the IEC61850 GOOSE
   protocol stack.
*/
/*stl includes */
#include <algorithm>
#include <utility>

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/application-layer.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/parser.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/pdu-offsets.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/object-reference-parser.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/time.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {

      /***** GSE Preamble implementation *****/
      gse_preamble::gse_preamble(vector<uint8_t> const& buffer) : valid_(false) {
    if ( buffer.size() < sizeof( gse_preamble_t ) )
      return;

    preamble_ = *reinterpret_cast<gse_preamble_t const*>(&buffer[0]);
    valid_ = true;
      }

      gse_preamble::gse_preamble(uint8_t const* buffer, size_t size) : valid_(false) {
    if ( size < sizeof( gse_preamble_t ) )
      return;

    preamble_ = *reinterpret_cast<gse_preamble_t const*>(buffer);
    valid_ = true;
      }

      bool gse_preamble::parse( vector<uint8_t> const& buffer ) {
        return (parse(&buffer[0], buffer.size()));
      }

      bool gse_preamble::parse( uint8_t const* buffer, size_t size ) {
        if ( size < sizeof( gse_preamble_t ) ) {
      valid_ = false;
      return false;
    }

    preamble_ = *reinterpret_cast<const gse_preamble_t*>(buffer);
    valid_ = true;
        return true;
      }

      void gse_preamble::serialize( uint8_t* write, uint8_t const* last ) {
        if ( sizeof(gse_preamble_t) > (last-write) ) {
          throw ccss_protocols::Exception
            ("Insufficient buffer space to serialize the preamble.");
        }

        std::copy(reinterpret_cast<uint8_t*>(&preamble_),
          reinterpret_cast<uint8_t*>(&preamble_)+sizeof(gse_preamble_t),
                  write);
      }

      namespace application {

        layer::layer() : update_dataset(nullptr), get_utc_time(get_utc_time_posix),
                         data_send_signal(), data_receive_signal(),
                         subscribed_data_sets_(), preamble_(), header_(),
                         publish_buffer_() {

        }

        layer::layer( std::function<void (string goCBRef, data_set& ds)> update_ds_callback ) :
          update_dataset(update_ds_callback), get_utc_time(get_utc_time_posix),
          data_send_signal(), data_receive_signal(), subscribed_data_sets_(),
          preamble_(), header_(), publish_buffer_() {

        }

    void layer::subscribe( data_set& ds ) {
          subscribed_data_sets_.insert
            ( std::pair<string, data_set>(ds.reference(), ds) );
    }

        void layer::subscribe( gocb& GoCBlock ) {
          subscribe( GoCBlock.dset );
        }

        void layer::un_subscribe( data_set& ds ) {
          subscribed_data_sets_.erase( ds.reference() );
        }

        void layer::un_subscribe( gocb& GoCBlock ) {
          un_subscribe( GoCBlock.dset );
        }

        void layer::publish( gocb& goCBlock, uint16_t appid/*= 0x0000*/) {

          /* construct the GSE preamble */
          goose::gse_preamble preamble;
          preamble.set_app_id( appid );
          preamble.set_length( 0 );
          preamble.set_reserved_one( 0 );
          preamble.set_reserved_two( 0 );

          /* lookup the related GOOSE message object (if it exists) */
          goose_msg* gmsg = nullptr;
          auto entry = goose_message_map_.find(goCBlock.GoCBRef());

          if ( entry == goose_message_map_.end() ) {
            // there is no entry for this control block, we will create one now
            goose_msg goose_message = {goCBlock.DatSet(), goCBlock.goID(), goCBlock.GoCBRef(),
                                       get_utc_time(), 1, 1, false, goCBlock.ConfRev(), goCBlock.NdsCom()};
            goose_message_map_.insert(std::make_pair(goCBlock.GoCBRef(), goose_message));
            gmsg = &(goose_message_map_.find(goCBlock.GoCBRef())->second);
          } else {
            gmsg = &(entry->second);

            if (goCBlock.dset.state_change()) {gmsg->StNum+=1; goCBlock.dset.state_change__(false);}
          }

          /* construct the GOOSE header based on the state variables in the goose_msg object */
          goose::header_t header;
          goose::header::field::set(header.goCBRef, gmsg->GoCBRef);
          goose::header::field::set(header.timeAllowedToLive, 2000);
          goose::header::field::set(header.datSet, gmsg->DatSet);
          goose::header::field::set(header.goID, gmsg->GoID);
          goose::header::field::set(header.T, get_utc_time());
          goose::header::field::set(header.stNum, gmsg->StNum);
          goose::header::field::set(header.sqNum, gmsg->SqNum); (gmsg->SqNum)+=1;
          goose::header::field::set(header.simulation, gmsg->Simulation);
          goose::header::field::set(header.confRev, gmsg->ConfRev);
          goose::header::field::set(header.ndsCom, gmsg->NdsCom);
          goose::header::field::set(header.numDatSetEntries, goCBlock.dset.num_entries());

          /* *******************************************************************************************/
          /* Need to retrieve the lengths of the different sections of the PDU in
             reverse order they appear on the wire.  By doing this we allow the
             lengths of the sections to be calculated with the tag and length
             extensions factored in */

          /* get total length of the 'Data Section' (including tag and length fields)*/
          size_t data_section_total_length =
            _get_section_total_length(goCBlock.dset.size());

          /* get total length of the 'GOOSE PDU' (not including tag and length fields)*/
          size_t goose_pdu_length =
            header::calculate_length(header) + data_section_total_length;

          /* get the total length of the GOOSE packet  */
          size_t goose_pdu_total_length =
            _get_section_total_length(goose_pdu_length);
          size_t preamble_length =
            goose_pdu_total_length + sizeof(gse_preamble_t);
          /* *******************************************************************************************/

          /***************************************************************************/
          /* Serialize all of the GOOSE PDU sections now that the length fields have
             been calculated and can be filled in */

          /* serialize the GSE preamble */
          preamble.set_length
            (static_cast<uint16_t>(preamble_length));
          preamble.serialize
            (&publish_buffer_[0], &publish_buffer_[GOOSE_PUBLISH_BUFF_SIZE]);

          /* serialize the GOOSE header */
          uint8_t* pdu_write_pos = &publish_buffer_[sizeof(gse_preamble_t)];

          *pdu_write_pos = GOOSE_HEADER_TAG;
          pdu_write_pos++;

          pdu_write_pos =
            _serialize_handle_ext_tag_len_value(pdu_write_pos, goose_pdu_length);

          pdu_write_pos =
            goose::header::serialize
            ( header, pdu_write_pos, &publish_buffer_[GOOSE_PUBLISH_BUFF_SIZE]);

          /* serialize the Data Section */
          *pdu_write_pos = GOOSE_DATA_SECTION_TAG;
          pdu_write_pos++;

          pdu_write_pos =
            _serialize_handle_ext_tag_len_value(pdu_write_pos, goCBlock.dset.size());

          if ( preamble_length < publish_buffer_.max_size() ) {
            for ( basic_value_t bval : goCBlock.dset.data ) {
          // add the tag value byte
          *pdu_write_pos = bval.type;
              pdu_write_pos++;

          // add the length length byte
              *pdu_write_pos = bval.val.size();
              pdu_write_pos++;

          // add the value to the publish buffer in network byte order (nbo)
              // NOTE: strings keep host byte order on the wire
              if ( bval.type == VISIBLE_STRING::tag_value ) {
                std::copy(bval.val.begin(), bval.val.end(), pdu_write_pos);
              } else {
                std::reverse_copy(bval.val.begin(), bval.val.end(), pdu_write_pos);
              }
              pdu_write_pos += bval.val.size();
        }
      }
          /***************************************************************************/

          /* send out the GOOSE message */
          data_send_signal(&publish_buffer_[0], preamble.length(), goCBlock.DstAddress());
        }

        bool layer::is_dataset_monitored( string& dataset_reference ) {
          if ( subscribed_data_sets_.find(dataset_reference) != subscribed_data_sets_.end() )
            return true;
          else
            return false;
        }

        data_set* layer::get_monitored_dataset( string& dataset_reference ) {
          if (!is_dataset_monitored( dataset_reference )) {
            return NULL;
          }

          return (&(subscribed_data_sets_.find(dataset_reference)->second));
        }

    /* signal handlers*/
    void layer::handle_data_receive( uint8_t* rx_buffer, size_t size ) {
      // parse the GOOSE preamble
          if ( !preamble_.parse(rx_buffer, size) ) {
            /* handle error here */
            return;
          }

      // parse the GOOSE PDU
      vector<basic_value_t> parsed_goose_msg;
          if ( !_parse_pdu(rx_buffer, size, header_, parsed_goose_msg) )
            /* handle error here */
        return;

      // 1. Has this DataSet (from header) been subscribed to?
      if ( !is_dataset_monitored(header_.datSet.value) ) {
        return;
          }


          // 2. Lookup the related GOOSE message object (if it exists)
          goose_msg* gmsg = nullptr;
          auto entry = goose_message_map_.find(header_.goCBRef.value);

          if ( entry == goose_message_map_.end() ) {
            // there is no entry for this control block, we will create one now
            goose_msg goose_message = {header_.datSet.value, header_.goID.value, header_.goCBRef.value,
                                       header_.T.value, header_.stNum.value, header_.sqNum.value,
                                       header_.simulation.value, header_.confRev.value, header_.ndsCom.value};
            goose_message_map_.insert(std::make_pair(header_.goCBRef.value, goose_message));
            gmsg = &(goose_message_map_.find(header_.goCBRef.value)->second);
          } else {
            gmsg = &(entry->second);
          }

          // 3. Check for a state change and update the parsed_dataset accordingly
          data_set parsed_dataset;
          if (gmsg->StNum < header_.stNum.value) {
            // there was a state change
            parsed_dataset.state_change__(true);
          } else {
            parsed_dataset.state_change__(false);
          }

          // 4. Update local goose message state with header values
          gmsg->StNum = header_.stNum.value;
          gmsg->SqNum = header_.sqNum.value;
          gmsg->Simulation = header_.simulation.value;
          gmsg->ConfRev = header_.confRev.value;
          gmsg->NdsCom = header_.ndsCom.value;

      // 5. Retrieve values from the last element in parsed_goose_msg
      //    (the data in the pdu)
      //    NOTE: The data wrapped in a tag -> length -> data format, so
      //          we need to parse the data from the 'val' variable
          parsed_dataset.reference(header_.datSet.value);

          basic_value_t unparsed_data = parsed_goose_msg.back();
      if ( !_parse_data(unparsed_data.val, parsed_dataset.data) )
        /* handle error here */
        return;

          // 6. verify that the number of data elements parsed matches the number
          // of data elements expected by the data set and that all the types
          // align as expected
          data_set* monitored_dataset =
            get_monitored_dataset(header_.datSet.value);

          if ( (parsed_dataset.data).size() == (monitored_dataset->data).size() ) {
            for (uint8_t i=0; i < (monitored_dataset->data).size(); ++i) {

              // verify that the types align
              if ( (parsed_dataset.data)[i].type != (monitored_dataset->data)[i].type ) {
                // if the types do not align, do not continue to process the parsed data
                return;
              }
            }
          }

          // 7. Notify the application that a dataset needs to be updated with
          //    the values in parsed_dataset.data
          if ( update_dataset != nullptr ) {
            update_dataset(header_.goCBRef.value, parsed_dataset);
          }
    }

    /* helper functions */
    uint16_t layer::_get_appid( vector<uint8_t> const& rx_buffer ) {
      return *(uint16_t*)&rx_buffer[0];
    }

    uint16_t layer::_get_length( vector<uint8_t> const& rx_buffer ) {
      return *(uint16_t*)&rx_buffer[2];
    }

        bool layer::_parse_pdu( uint8_t const* rx_buffer, size_t size, header_t& hdr, vector<basic_value_t>& parsed_triplets ) {

          uint8_t const* data_start = rx_buffer;
          uint8_t const* data_end   = rx_buffer + size;

      //   NOTE: The first two bytes in the packet (after the preamble)
      //   represent the GOOSE header tag.  That tag can be one of two
      //   values.  One signifies GOOSEv1 with length byte, and one signifies
      //   GOOSEv2 w/o length byte. TODO: UPDATE THIS NOTE (ITS WRONG)

      uint16_t pdu_length = 0x00;
      // verify the GOOSE header tag id
      if ( rx_buffer[GOOSE_MESSAGE_TAG_OFFSET] != GOOSE_HEADER_TAG) {
        return false;
      }

      uint8_t length_ext_field = rx_buffer[GOOSE_MESSAGE_TAG_OFFSET + 1];
      if ( length_ext_field < 0x7F ) {
        // store the length of the rest of the GOOSE pdu
        pdu_length = length_ext_field;

        data_start += (GOOSE_MESSAGE_TAG_OFFSET + 2);
      }
      else if ( length_ext_field == 0x81 ) {
        // store the length of the rest of the GOOSE pdu
        pdu_length = rx_buffer[GOOSE_MESSAGE_TAG_OFFSET + 2];

        data_start += (GOOSE_MESSAGE_TAG_OFFSET + 3);
      }
      else if ( length_ext_field == 0x82 ) {
        // store the length of the rest of the GOOSE pdu
        pdu_length =
          ntohs(*reinterpret_cast<uint16_t const*>(&rx_buffer[GOOSE_MESSAGE_TAG_OFFSET + 2]));

        data_start += (GOOSE_MESSAGE_TAG_OFFSET + 4);
      }
      else {
        // TODO: handle error here
        return false;
      }

      /**
          Use the boost::spirit compile-time-generated parser to parse the
          receive buffer
      */
          goose_grammar<uint8_t const*> grammar;

      bool success = qi::parse(data_start, data_end, grammar, parsed_triplets);
      if ( (!success) || (data_start != data_end) ) {
        /* parsing failed */
        return false;
      }
      /*************************************************************************/

      // verify that what was parsed from the wire at least contains enough
      // data to be a GOOSE header
      const uint8_t GOOSE_HDR_FIELD_COUNT = 11;
      if ( parsed_triplets.size() < GOOSE_HDR_FIELD_COUNT )
        return false;

      /**
         Each entry in the triplets container now holds the tag and value
         retrieved from the off-wire-buffer and parsed using the above
         "parse()" function. The first 11 entries correspond (in order) with
         the entries in the header_t struct that represtent the GOOSE message
         header.  Everything following will be data.
      */
      if ( !_parse_header(parsed_triplets, hdr) )
        return false;

      return true;
    }

    bool layer::_parse_header( vector<basic_value_t> const& triplets, header_t& hdr ) {
      if (triplets[0].type == goCBRef_t::tag_value)
        hdr.goCBRef.value.assign
          (reinterpret_cast<const char*>(&triplets[0].val[0]), triplets[0].val.size());
      else
        return false;

      if (triplets[1].type == timeAllowedToLive_t::tag_value) {
            hdr.timeAllowedToLive.value = 0;
            for (uint8_t const x : triplets[1].val) {
              hdr.timeAllowedToLive.value = (hdr.timeAllowedToLive.value << 8) | x;
            }
          }
          else
            return false;

          if (triplets[2].type == datSet_t::tag_value)
            hdr.datSet.value.assign
              (reinterpret_cast<const char*>(&triplets[2].val[0]), triplets[2].val.size());
          else
            return false;

          if (triplets[3].type == goID_t::tag_value)
            hdr.goID.value.assign
              (reinterpret_cast<const char*>(&triplets[3].val[0]), triplets[3].val.size());
          else
            return false;

          if (triplets[4].type == T_t::tag_value)
            hdr.T.value =
              *reinterpret_cast<const UTC_TIME_T*>(&triplets[4].val[0]);
      else
        return false;

      if (triplets[5].type == stNum_t::tag_value) {
            hdr.stNum.value = 0;
            for (uint8_t const x : triplets[5].val) {
              hdr.stNum.value = (hdr.stNum.value << 8) | x;
            }
          }
      else
        return false;

      if (triplets[6].type == sqNum_t::tag_value) {
            hdr.sqNum.value = 0;
            for (uint8_t const x : triplets[6].val) {
              hdr.sqNum.value = (hdr.sqNum.value << 8) | x;
            }
          }
      else
        return false;

      if (triplets[7].type == simulation_t::tag_value)
        hdr.simulation.value = triplets[7].val[0];
      else
        return false;

      if (triplets[8].type == confRev_t::tag_value) {
            hdr.confRev.value = 0;
            for (uint8_t const x : triplets[8].val) {
              hdr.confRev.value = (hdr.confRev.value << 8) | x;
            }
          }
      else
        return false;

      if (triplets[9].type == ndsCom_t::tag_value)
        hdr.ndsCom.value = triplets[9].val[0];
      else
        return false;

      if (triplets[10].type == numDatSetEntries_t::tag_value) {
            hdr.numDatSetEntries.value = 0;
            for (uint8_t const x : triplets[10].val) {
              hdr.numDatSetEntries.value = (hdr.numDatSetEntries.value << 8) | x;
            }
          }
      else
        return false;

      return true;
    }

    bool layer::_parse_data( vector<uint8_t> const& rx_buffer, vector<basic_value_t>& parsed_triplets ) {
      /**
          Use the boost::spirit compile-time-generated parser to parse the
          receive buffer
      */
      vector<uint8_t>::const_iterator
        data_start(rx_buffer.begin()),
        data_end(rx_buffer.end());

      goose_grammar<vector<uint8_t>::const_iterator> grammar;

      bool success = qi::parse(data_start, data_end, grammar, parsed_triplets);
      if ( (!success) || (data_start != data_end) ) {
        /* parsing failed */
        return false;
      }
      /*************************************************************************/

      return true;
    }

        size_t layer::_get_section_total_length(size_t const check_length) {
          size_t length = _get_tag_length_ext_size(check_length);

          // add one byte for the section identifier tag
          length++;

          return length;
        }

        size_t layer::_get_tag_length_ext_size(size_t const check_length) {
          size_t length = check_length;

          /* check if the extended tag/value fields for the GOOSE PDU should be used */
          if (check_length > MAX_SIZE_UNEXTENDED) {
            if (check_length <= 0xFF) { // max size possible with a single byte
              // add tag extension byte to length
              length++;

              // add length byte to length
              length++;
            } else {
              // add tag extension byte to length
              length++;

              // add two length bytes to length
              length += 2;
            }
          } else {
            // add the GOOSE PDU length
            length++;
          }

          return length;
        }

        uint8_t* layer::_serialize_handle_ext_tag_len_value(uint8_t* write, size_t check_length) {
          /* check if the extended tag/value fields for the GOOSE PDU should be used */
          if (check_length > MAX_SIZE_UNEXTENDED) {
            if (check_length <= 0xFF) { // max size possible with a single byte
              // add tag extension indicating single byte length
              *write = GOOSE_TAG_EXT1;
              write++;

              // add length byte
              *write = static_cast<uint8_t>(check_length);
              write++;
            } else {
              // add tag extension indicating two byte length
              *write = GOOSE_TAG_EXT2;
              write++;

              // add length first byte in (nbo)
              *write = *(reinterpret_cast<uint8_t*>(&check_length) + 1);
              write++;

              // add length second byte in (nbo)
              *write = *reinterpret_cast<uint8_t*>(&check_length);
              write++;
            }
          } else {
            // add the GOOSE PDU length
            *write = static_cast<uint8_t>(check_length);
            write++;
          }

          return write;
        }

      } // namespace application

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_protocols
