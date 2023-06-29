/**
   @brief An IEC61850 GOOSE subscriber outstation
*/
#ifndef __IEC61850_GOOSE_SUBSCRIBER_HPP__
#define __IEC61850_GOOSE_SUBSCRIBER_HPP__

/* stl includes */
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

/* boost includes */
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/utility.hpp>

/* posix includes */
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h> // L2 protocols
#include <netpacket/packet.h>

#include "bennu/devices/modules/comms/iec61850/device/attribute-map.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/protocol-stack.hpp"

namespace ccss_devices {
  namespace iec61850 {
    namespace goose {

      using std::map;
      using std::string;
      using std::vector;
      using std::uint8_t;
      using std::uint32_t;
      using std::multimap;

      using namespace ccss_protocols::iec61850;
      using namespace ccss_protocols::iec61850::goose;

      typedef std::function<void (data_set& ds)> subscription_callback_fn_t;

      /**
         @brief This class implements a generic IEC 61850 subscriber

         By inheriting from noncopyable, the instantiated subscriber object cannot be copied
      */
      class subscriber : virtual public attribute_map, private boost::noncopyable {
      public:
        subscriber(const char* iface_name);

        /**@brief Subscribe to a GOOSE data set

           @param ds The dataset that the user wants to subscribe to.  This parameter
           contains the dataset reference to listen for on the wire as well as telling
           the GOOSE protocol stack what the structure of the data looks like.
           @param cb_fn Callback to be called by the subscriber object when the specified
           dataset is seen on the wire.*/
        void subscribe( data_set& ds, subscription_callback_fn_t cb_fn );

        /**@brief Subscribe to a GOOSE data set by passing in the GOOSE control block
           that manages it.

           @param GoCBlock The GOOSE control-block managing the dataset that the user
           wants to subscribe to.  This parameter contains the dataset reference to
           listen for on the wire as well as telling the GOOSE protocol stack what
           the structure of the data looks like.
           @param cb_fn Callback to be called by the subscriber object when the specified
           dataset is seen on the wire.*/
        void subscribe( gocb& GoCBlock, subscription_callback_fn_t cb_fn );

        /**@brief Un-Subscribe from a GOOSE data set

           @param ds The dataset that the user wants to un-subscribe from.*/
        void un_subscribe( data_set& ds );

        /**@brief Un-Subscribe from a GOOSE data set by passing in the GOOSE control block
           that manages it.

           @param GoCBlock The GOOSE control-block managing the dataset that the user
           wants to un-subscribe from.*/
        void un_subscribe( gocb& GoCBlock );

        /**@brief Callback to be called by GOOSE protocol stack when a dataset that
           has been subscribed to has been published.
           @param goCBRef GOOSE control block reference
           @param dataset dataset that is to be updated*/
        void update_dataset(string goCBRef, data_set& dataset_ref);

        /** @brief Halt the outstation

            This function will end all subscription threads.  All subscription
            data will be purged.  After this call is made the user will need to
            rebuild all subscriptions  manually or by reloading a SCL CID
            configuration file.
         */
        void halt( void );

        /** @brief Loads CID configuration file and sets up all subscriptions

            @param filename Name of the CID file to load
            @param subscription_cb Function callback that is called when a subscribed dataset is received
        */
        void configure( string const& filename, subscription_callback_fn_t subscription_cb );

        /** @brief Loads CID configuration file

            @param filename Name of the CID file to load
            @param subs Subscritpion list (output parameter)
        */
        void import_config( string const &filename );
      private:
        /**@brief Start the GOOSE subscription thread

           Running the subscriber will start the outstation listening for datasets
           on the wire that the user has subscribed to.  This function acts as the
           running context for reading data off the wire and passing data to the
           protocol stack*/
        void run( void );

        /**@brief Called when the subscriber wants to send data

           This function is also registered with a conversation instance as
           a way for the protocol stack to send data when it needs to.
        */
        void send( uint8_t* tx_buffer, size_t size );

        /**@brief Called when the protocol stack wants to wait for incoming data

           This function is also registered with a conversation instance as
           a way for the protocol stack to ask for data when it needs it.
        */
        void receive( uint8_t* rx_buffer, size_t size );

        /**@brief This function retrieves a complete GOOSE message from the wire*/
        bool retrieve_packet( vector<uint8_t>& packet );

      public:
        // Listing of all subscribed data-sets by data-set reference
        map<string, data_set> subscriptions;

      private:
        // Ethernet communication variables
        int raw_socket_;
        string iface_name_;

        // GOOSE protocol_stack object
        goose::protocol_stack goose_stack_;

        // Subscription notification callback map
        multimap<string, subscription_callback_fn_t> subscription_cb_map_;

        // Subscription thread
        boost::thread* subscription_thread_;
      };

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_devices

namespace detail {
  int get_interface_index_(int const& sockfd, const char* interface_name);
  inline void set_interface_promisc_(int const& sockfd, const char* interface_name);
  inline void bind_interface_(int const& sockfd, const char* interface_name);
} // namespace detail

#endif /* __IEC61850_GOOSE_SUBSCRIBER_HPP__ */
