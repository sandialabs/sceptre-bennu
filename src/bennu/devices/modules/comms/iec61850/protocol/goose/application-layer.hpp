/**
   @brief This file contains the API for the IEC61850 GOOSE application
   layer
*/
#ifndef __IEC61850_GOOSE_APPLICATION_LAYER_HPP__
#define __IEC61850_GOOSE_APPLICATION_LAYER_HPP__

/* stl includes */
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

/* posix / winsock includes */
#ifdef _WIN
#include <winsock2.h>   // included for ntohs() / htons()
#else
#include <arpa/inet.h>  // included for ntohs() / htons()
#endif

/* boost includes */
#include <boost/signals2.hpp>

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/config.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/data-set.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/gocb.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/header.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/message.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {

      using std::map;
      using std::string;
      using std::uint16_t;

      struct gse_preamble_t {
    uint16_t app_id;       /**<application id*/
    uint16_t length;       /**<length of the whole GOOSE packet*/
    uint16_t reserved_one; /**<reserved*/
    uint16_t reserved_two; /**<reserved*/
      };

      /**
     @brief GOOSE preamble definition

     This class provides an API that will perform data field accesses using
     the correct byte orientation for a GOOSE message preamble.  The
     internal data store for a GOOSE message preamble is always in network
     byte order.
      */
      class gse_preamble {
      public:
    gse_preamble() : valid_(false) {}
    gse_preamble(vector<uint8_t> const& buffer);
        gse_preamble(uint8_t const* buffer, size_t size);

    bool parse( vector<uint8_t> const& buffer );
    bool parse( uint8_t const* buffer, size_t size );

    bool is_valid( void ) { return valid_; }

    void set_app_id( uint16_t app_id )    { preamble_.app_id = htons( app_id ); }
    void set_length( uint16_t length )    { preamble_.length = htons( length ); }
    void set_reserved_one( uint16_t one ) { preamble_.reserved_one = htons( one ); }
    void set_reserved_two( uint16_t two ) { preamble_.reserved_two = htons( two ); }

    uint16_t app_id( void )       { return ntohs(preamble_.app_id); }
        uint16_t length( void )       { return ntohs(preamble_.length); }
    uint16_t reserved_one( void ) { return ntohs(preamble_.reserved_one); }
    uint16_t reserved_two( void ) { return ntohs(preamble_.reserved_two); }

        gse_preamble_t get( void ) { return preamble_; }

        void serialize( uint8_t* write, uint8_t const* last );
      private:
    gse_preamble_t preamble_;
    bool valid_;
      };

      namespace application {

    using std::string;
    using std::vector;
    using std::uint8_t;

    using boost::signals2::signal;

    const uint16_t GOOSE_ETHER_TYPE = 0x88B8;

        const uint8_t GOOSE_DATA_SECTION_TAG = 0xAB;

        /**< Max size of a GOOSE PDU triplet 'value' before the use of extended tag semantics */
        const uint8_t MAX_SIZE_UNEXTENDED = 0x7F;

        /**< Tag extension value for a GOOSE PDU indicating single byte length field*/
        const uint8_t GOOSE_TAG_EXT1 = 0x81;

        /**< Tag extension value for a GOOSE PDU indicating two byte length field*/
        const uint8_t GOOSE_TAG_EXT2 = 0x82;

    class layer {
    public:
          layer();
          layer( std::function<void (string goCBRef, data_set& ds)> update_ds_callback );

      /* application layer API */

      /**@brief Subscribe to a dataset
         @param ds  the dataset to subscribe to*/
      void subscribe( data_set& ds );

          /**@brief Subscribe to a dataset managed by a GOOSE control-block

             Subscription is still done according to the dataset reference
             of the dataset that is managed.

         @param GoCBlock GOOSE control-block the dataset to subscribe to is
             managed by*/
      void subscribe( gocb& GoCBlock );

          /**@brief Un-Subscribe from a dataset
         @param ds the dataset to un-subscribe from*/
          void un_subscribe( data_set& ds );

          /**@brief Un-Subscribe from a dataset managed by a GOOSE control-block

             Subscription is still done according to the dataset reference
             of the dataset that is managed.

         @param GoCBlock GOOSE control-block the dataset to un-subscribe from
             is managed by*/
          void un_subscribe( gocb& GoCBlock );

          /**@brief Publish a local device GOOSE control block
             @param goCBlock GOOSE control-block managing the data set that is to be published
             @param appid Application ID being used by the GOOSE control block*/
          void publish( gocb& goCBlock, uint16_t appid = 0x0000 );

          /**@brief Check if a dataset has been subscribed to
             @param dataset_reference Reference to the dataset
             @return true if it has been subscribed to, false otherwise */
          bool is_dataset_monitored( string& dataset_reference );

          /**@brief Retrieve a pointer to a monitored dataset
             @param dataset_reference Reference to the dataset
             @return dataset */
          data_set* get_monitored_dataset( string& dataset_reference );

      /**@brief Return a pointer to the parsed preamble
         @return Pointer to the parsed preamble on success, NULL on failure*/
      gse_preamble const* preamble() { return &preamble_; }

      /**@brief Return a pointer to the parsed header
         @return Pointer to the parsed header on success, NULL on failure */
      header_t const* header() { return &header_; }

          /* application callbacks */
          std::function<void ( string goCBRef, data_set& )> update_dataset;

          /* current time retrieval callback */
          std::function<UTC_TIME_T (void)> get_utc_time;

      /* signals */
      signal<void ( uint8_t*, size_t, uint8_t* )> data_send_signal;
      signal<void ( uint8_t*, size_t, uint8_t* )> data_receive_signal;

      /* signal handlers*/
      void handle_data_receive( uint8_t* rx_buffer, size_t size );

      /* helper functions */
      uint16_t _get_appid( vector<uint8_t> const& rx_buffer );
      uint16_t _get_length( vector<uint8_t> const& rx_buffer );

      bool _parse_pdu( vector<uint8_t> const& rx_buffer, header_t& hdr, vector<basic_value_t>& parsed_triplets );
          bool _parse_pdu( uint8_t const* rx_buffer, size_t size, header_t& hdr, vector<basic_value_t>& parsed_triplets );

    private:
      /**@brief Function to parse a GOOSE header from the data structure returned
         in the output parameter 'parsed_triplets' from _parse_pdu()

         This function assumes the first through eleventh triplets align with
         the fields in the GOOSE header.  The function is private due to this
         assumption (protection from misuse). */
      bool _parse_header( vector<basic_value_t> const& triplets, header_t& hdr );

      /**@brief Function to parse a GOOSE data from raw serialized bytes*/
      bool _parse_data( vector<uint8_t> const& rx_buffer, vector<basic_value_t>& parsed_triplets );

          /**@brief Write the tag/length fields in the GOOSE PDU for vaious triplets.

           This function takes care of the tag and length extensions if need be.
           It expectes check_length to be the size of the 'value' field not counting
           the 'tag' and 'length' bytes*/
          inline uint8_t* _serialize_handle_ext_tag_len_value( uint8_t* write, size_t check_length );

          /**@brief Retrieve the size of a section after calculating the
             tag and length extensions (if applicable)*/
          inline size_t _get_section_total_length(size_t const check_length);
          inline size_t _get_tag_length_ext_size(size_t const check_length);

        private:
      /*Data structures resulting from a call to handle_data_recieve(). They
         will be overwritten with each subsequent call*/
      gse_preamble preamble_;
      header_t     header_;

          /*Mapping between goose message objects and goose control blocks for
            publishing.*/
          map<string, goose_msg> goose_message_map_;

          /*This is where we will maintain our monitored data-sets for the
         time being. */
          map<string, data_set> subscribed_data_sets_;

          /*Buffer for serializing and publishing data. Overwritten each time
            publish() is called*/
          std::array<uint8_t, GOOSE_PUBLISH_BUFF_SIZE> publish_buffer_;
    };

      } // namespace application

    } // namespace goose

  } //namespace iec61850

} // namespace ccss_protocols

#endif  /* __IEC61850_GOOSE_APPLICATION_LAYER_HPP__ */
