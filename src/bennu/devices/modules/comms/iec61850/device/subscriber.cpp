/**
   @brief Implementation for the GOOSE subscriber outstation defined in subscriber.hpp
*/
/* stl includes */
#include <functional>
#include <map>
#include <string>
#include <vector>

/* posix includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h> // L2 protocols
#include <netpacket/packet.h>

/* boost includes */
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "bennu/devices/modules/comms/iec61850/device/static_vector.hpp"
#include "bennu/devices/modules/comms/iec61850/device/subscriber.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"

namespace ccss_devices {
  namespace iec61850 {
    namespace goose {

      using std::map;
      using std::pair;
      using std::vector;
      using std::string;
      using std::uint8_t;

      using std::placeholders::_1;
      using std::placeholders::_2;

      using namespace ccss_protocols::iec61850;

      using ccss_protocols::utility::static_vector;
      using ccss_protocols::iec61850::goose::application::GOOSE_ETHER_TYPE;

      subscriber::subscriber(const char* iface_name) : raw_socket_(), iface_name_(iface_name),
                                                       goose_stack_(), subscription_cb_map_(),
                                                       subscription_thread_(nullptr) {
        /* initialize the raw socket file handle */
        raw_socket_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

        if (!raw_socket_)
          throw ccss_protocols::Exception("Error: Unable to obtain a socket file handle");

        /* put the interface in promiscuous mode */
        detail::set_interface_promisc_(raw_socket_, iface_name);

        /* bind socket to specific interface */
        detail::bind_interface_(raw_socket_, iface_name);

        /* register all callbacks with the GOOSE protocol stack */
        goose_stack_.app_layer.update_dataset =
          std::bind(&subscriber::update_dataset, std::ref(*this), _1, _2);
      }

      void subscriber::run(void) {

        // Raw data receive buffer
        vector<uint8_t> rx_buffer;

        try {

          while (1) {

            // check to see if an interruption has been requested
            boost::this_thread::interruption_point();

            // attempt to retrieve a packet from the wire
            if (retrieve_packet(rx_buffer)) {
              // pass the data to the GOOSE protocol_stack
              goose_stack_.data_receive_signal( &rx_buffer[0], rx_buffer.size() );
            }

          }
        }
        catch ( ccss_protocols::Exception& e ) {
          std::cerr << "GOOSE SUBSCRIBER: iec61850 exception: " << e.what() << std::endl;
        }
        catch ( std::exception& e ) {
          std::cerr << "GOOSE SUBSCRIBER: std::exception: " << e.what() << std::endl;
        }
        catch ( ... ) {
          std::cerr << "GOOSE SUBSCRIBER: Caught a thrown non-exception.  Bad form!" << std::endl;
        }
      }

      void subscriber::subscribe( data_set& ds, subscription_callback_fn_t cb_fn ) {
        /* tell the protocol stack to subscribe to a dataset according to its
           published reference and type layout */
        goose_stack_.app_layer.subscribe( ds );

        /* keep track of the subscription */
        if (cb_fn != nullptr) {
          subscription_cb_map_.insert(std::make_pair(ds.reference(), cb_fn));
        }

        subscriptions.insert(std::make_pair(ds.reference(), ds));

        /* start the subscription thread if it is not already running */
        if (!subscription_thread_) {
          subscription_thread_ = new boost::thread(std::bind(&subscriber::run, std::ref(*this)));
        }
      }

      void subscriber::subscribe( gocb& GoCBlock, subscription_callback_fn_t cb_fn ) {
        subscribe( GoCBlock.dset, cb_fn );
      }

      void subscriber::un_subscribe( data_set& ds ) {
        goose_stack_.app_layer.un_subscribe( ds );
        subscriptions.erase(ds.reference());
      }

      void subscriber::un_subscribe( gocb& GoCBlock ) {
        un_subscribe( GoCBlock.dset );
      }

      void subscriber::update_dataset( string goCBRef, data_set& ds ) {
        // Allow registered user callbacks to manage the dataset
        pair<multimap<string, subscription_callback_fn_t>::iterator,
             multimap<string, subscription_callback_fn_t>::iterator> range;
        range = subscription_cb_map_.equal_range(ds.reference());

        multimap<string, subscription_callback_fn_t>::iterator callback_iter = subscription_cb_map_.find(ds.reference());
        for( callback_iter = range.first; callback_iter != range.second; ++callback_iter) {
          callback_iter->second( ds );
        }
      }

      void subscriber::halt( void ) {
        // un-subscribe from all datasets
        for (auto entry : subscriptions) {
          un_subscribe(entry.second);
        }

        // delete the subscription callback map
        subscription_cb_map_.clear();

        // kill the subscription thread
        subscription_thread_->interrupt();
      }

      bool subscriber::retrieve_packet( vector<uint8_t>& packet ) {
        /**
            Because the iec61850 protocol stack expects a complete packet to
            be passed to it for parsing, we need to read the length octet from
            the OS buffer and then ask for exactly that much data from the next
            read.  This allows the OS to do all of the buffering.
        */

        /********************Filter on GOOSE Messages******************/
        const uint8_t ether_addr_size     = 0x06; // 6 bytes
        const uint8_t ether_type_size     = 0x02; // 2 bytes
        const uint8_t ether_hdr_size      = (ether_addr_size*2) + ether_type_size;
        const uint8_t ether_hdr_vlan_size = ether_hdr_size + 0x04;

        const uint16_t max_datagram_size = 0xFFFF; // 65535 bytes

        const uint8_t ETHER_SOURCE_OFFSET    = 0x00;
        const uint8_t ETHER_DEST_OFFSET      = 0x06;
        const uint8_t ETHER_TYPE_OFFSET      = 0x0C;

        const uint16_t VLAN_ETHER_TYPE        = 0x8100;
        const uint8_t  VLAN_ETHER_TYPE_OFFSET = ETHER_TYPE_OFFSET + 0x04;
        const uint8_t  VLAN_GOOSE_OFFSET      = VLAN_ETHER_TYPE_OFFSET + 0x04;

        // buffer to hold datagram
        static_vector<uint8_t, max_datagram_size> datagram;

        // read the entire datagram from the GOOSE socket
        uint16_t datagram_read_len = recvfrom(raw_socket_, datagram.data(), max_datagram_size, 0, NULL, NULL);

        // verify that the packet is a GOOSE message
        if ( datagram_read_len > ETHER_TYPE_OFFSET ) {
          if ( GOOSE_ETHER_TYPE != ntohs(*reinterpret_cast<uint16_t*>(&datagram[ETHER_TYPE_OFFSET]))) {
            // check if this is a GOOSE message passed over a VLAN
            if ( VLAN_ETHER_TYPE == ntohs(*reinterpret_cast<uint16_t*>(&datagram[ETHER_TYPE_OFFSET])) ) {
              if ( GOOSE_ETHER_TYPE != ntohs(*reinterpret_cast<uint16_t*>(&datagram[VLAN_ETHER_TYPE_OFFSET])))
                return false;
              else {
                /********************Process GOOSE Message over VLAN************************/
                // read the GOOSE message size
                uint16_t goose_msg_size = ntohs(*(reinterpret_cast<uint16_t*>(&datagram[VLAN_GOOSE_OFFSET])));

                // resize the packet buffer appropriately
                packet.resize(goose_msg_size);

                // copy the GOOSE message into the data buffer
                std::copy((datagram.data()+ether_hdr_vlan_size),
                          (datagram.data()+(ether_hdr_vlan_size+goose_msg_size)),
                          &packet[0]);
                /***************************************************************************/
              }
            }
            else
              return false;
          }
          else {
            /********************Process GOOSE Message************************/
            // read the GOOSE message size
            uint16_t goose_msg_size = ntohs(*(reinterpret_cast<uint16_t*>(&datagram[ether_hdr_size+2])));

            // resize the packet buffer appropriately
            packet.resize(goose_msg_size);

            // copy the GOOSE message into the data buffer
            std::copy((datagram.data()+ether_hdr_size),
                      (datagram.data()+(ether_hdr_size+goose_msg_size)),
                      &packet[0]);
            /******************************************************************/
          }
        }

        return true;
      }

      void subscriber::send( uint8_t* tx_buffer, size_t size ) {}

      void subscriber::receive( uint8_t* rx_buffer, size_t size ) {
        uint16_t len;
        len = recvfrom(raw_socket_, rx_buffer, size, 0, NULL, NULL);

        if ( len != size )
          throw ccss_protocols::Exception("Invalid GOOSE Message");
      }

      void subscriber::configure( string const& filename, subscription_callback_fn_t subscription_cb ) {
        // process input CID file
        import_config(filename);

        // handle subscriptions
        for (auto& sub : subscriptions) {
          subscribe(sub.second, subscription_cb);
        }
      }

      void subscriber::import_config( string const &filename ) {
        using namespace detail;
        using boost::property_tree::ptree;

        ptree pt;
        string ld_name, datSetName;

        // Read in XML config file
        read_xml(filename, pt);

        /*
          Accessing other CID file tags:
          --------------------------------
          subscriber::SEL_GooseSubscription gSub;
          gSub.iedName = v.second.get<string>("esel:GooseSubscription.<xmlattr>.iedName");
          gSub.ldInst = v.second.get<string>("esel:GooseSubscription.<xmlattr>.ldInst");
          gSub.cbName = v.second.get<string>("esel:GooseSubscription.<xmlattr>.cbName");
          gSub.datSet = v.second.get<string>("esel:GooseSubscription.<xmlattr>.datSet");
          gSub.confRev = v.second.get<uint32_t>("esel:GooseSubscription.<xmlattr>.confRev");
          gSub.mAddr = v.second.get<string>("esel:GooseSubscription.<xmlattr>.mAddr");
          gSub.APPID = v.second.get<uint32_t>("esel:GooseSubscription.<xmlattr>.APPID");
          gSub.vlanId = v.second.get<uint32_t>("esel:GooseSubscription.<xmlattr>.VLAN-ID");
          gSub.vlanPriority = v.second.get<uint32_t>("esel:GooseSubscription.<xmlattr>.VLAN-PRIORITY");
          gSub.appId = v.second.get<string>("esel:GooseSubscription.<xmlattr>.appId");
          gSub.goCbRef = v.second.get<string>("esel:GooseSubscription.<xmlattr>.goCbRef");
          gSub.datSetRef = v.second.get<string>("esel:GooseSubscription.<xmlattr>.datSetRef");
         */
        BOOST_FOREACH(ptree::value_type &v, pt.get_child("SCL.IED")) {
          // Search for GOOSE subscription
          // SEL uses proprietary tags inside "Private" blocks. They are located using the
          // "type=SEL_GooseSubscription" attribute of "Private"
          if(v.first == "Private") {
            if(v.second.size() > 1 && v.second.get<string>("<xmlattr>.type") == "SEL_GooseSubscription") {
              string datSetName = v.second.get<string>("esel:GooseSubscription.<xmlattr>.datSet");
              string datSetRef = v.second.get<string>("esel:GooseSubscription.<xmlattr>.datSetRef");
              data_set dset(datSetName, datSetRef);
              BOOST_FOREACH(ptree::value_type &v2, v.second.get_child("esel:GooseSubscription")) {
                // We assume each "GooseRxEntry" corresponds to a specific data attribute
                if(v2.first == "GooseRxEntry") {
                  string attr = v2.second.get<string>("<xmlattr>.tdlString");
                  // We assume VB stands for "Virtual Bit" and will always be a Boolean value
                  if (attr.find("VB") != string::npos) {
                    dset.add_attribute<Boolean>();
                  }
                  // We assume RA stands for "Real ?" and will always be a float value
                  else if (attr.find("RA") != string::npos) {
                    dset.add_attribute<FLOAT32>();
                  }
                }
              }

              // add dataset to the subscription list
              subscriptions.insert( std::pair<string, data_set>(dset.reference(), dset) );
            }
          }
        }
      }

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_devices

namespace detail {
  int get_interface_index_(int const& sockfd, const char *interface_name) {
    struct ifreq ifr;
    int fd;

    memset(&ifr, 0, sizeof(ifr));

    // setup ifr for ioctl
    strncpy (ifr.ifr_name, interface_name, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';

    // get index
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
      return (-1);
    }

    return ifr.ifr_ifindex;
  }

  void set_interface_promisc_(int const& sockfd, const char *interface_name) {
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));

    // setup ifr for ioctl
    strncpy (ifr.ifr_name, interface_name, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';

    // get interface flags
    if ( ioctl (sockfd, SIOCGIFFLAGS, &ifr) < -1) {
      throw ccss_protocols::Exception("Error: Unable to obtain a socket flags from the device.");
      exit( 1 );
    }

    // set the old flags plus the IFF_PROMISC flag
    ifr.ifr_flags |= IFF_PROMISC;
    if ( ioctl(sockfd, SIOCSIFFLAGS, &ifr) == -1)
      {
        throw ccss_protocols::Exception("Error: Could not set flag IFF_PROMISC.\n" );
        exit( 1 );
      }
  }

  void bind_interface_(int const& sockfd, const char* interface_name) {
    struct sockaddr_ll sll;
    memset( &sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = get_interface_index_(sockfd, interface_name);
    sll.sll_protocol = htons(ETH_P_ALL);

    if ( ( bind(sockfd, ( struct sockaddr* )&sll, sizeof( sll ) ) ) < 0 ) {
      throw ccss_protocols::Exception("Error: Failed to bind socket to interface.");
      exit( -1 );
    }
  }
} // namespace detail
