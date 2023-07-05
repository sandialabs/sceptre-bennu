/**
   @brief Implementation of the API defined in publisher.hpp
*/
/* stl includes */
#include <functional>
#include <string>

/* boost includes */
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

/* posix includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <net/ethernet.h> // L2 protocols
#include <netpacket/packet.h>

#include "bennu/devices/modules/comms/iec61850/device/publisher.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"

namespace ccss_devices {
  namespace iec61850 {
    namespace goose {

      using std::string;

      using std::placeholders::_1;
      using std::placeholders::_2;
      using std::placeholders::_3;

      using boost::asio::deadline_timer;

      using ccss_protocols::iec61850::goose::application::GOOSE_ETHER_TYPE;

      publisher::publisher(const char* iface_name)  : io_(), raw_socket_(0), iface_index_(-1), iface_name_(iface_name),
                                                      goose_sopts_(), goose_session_(nullptr) {

        /* initialize the GOOSE session options and the goose session */
        goose_sopts_.transmit_fn =
          std::bind(&publisher::low_level_send_, std::ref(*this), _1, _2, _3);
        goose_session_ = new goose::session(goose_sopts_);

        /* initialize the raw socket file handle */
        raw_socket_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

        if (!raw_socket_)
          throw ccss_protocols::Exception("Unable to obtain a socket file handle");

        /* set the interface index number */
        iface_index_ = get_interface_index_(iface_name);

        if (iface_index_ == -1)
          throw ccss_protocols::Exception("Unable to obtain an interface index number");

        /* set the interface hw address */
        iface_addr_ = get_interface_address_(iface_name_.c_str());

        if (iface_addr_.size() != 6)
          throw ccss_protocols::Exception("Unable to obtain an interface hardware address ");
      }

      void publisher::publish( gocb& goCBlock, uint16_t appid /*= 0x0000*/ ) {
        // if publish is being called from a scheduled thread, check to see if an interruption
        // has been requested before processing another publication
        boost::this_thread::interruption_point();

        // call the GOOSE protocol stack's publish() function
        //goose::session goose_session(goose_sopts_);
        goose_session_ -> app_layer.publish( goCBlock, appid );
      }

      bool publisher::schedule( gocb& goCBlock, uint32_t time_interval,
                                time_unit_t::type time_unit ) {
        // ensure that this dataset is not already being published
        if ( schedule_.find( goCBlock.dset.reference() ) != schedule_.end() ) {
          // this dataset is already being published
          return false;
        }

        // this line is a bit ugly.  because publish_() is overloaded, i have to type-cast
        // in order to disambiguate which one i am attempting to bind
        boost::thread* publish_thread =
          new boost::thread(std::bind
                            (static_cast<void(publisher::*)(gocb*,uint32_t,time_unit_t::type)>(&publisher::publish_),
                             std::ref(*this), &goCBlock, time_interval, time_unit));

        // track the publication
        publications.insert(std::make_pair(goCBlock.dset.reference(), goCBlock));

        // add a reference to the dataset in the schedule
        schedule_.insert( std::pair<string, boost::thread*>(goCBlock.dset.reference(), publish_thread) );

        return true;
      }

      bool publisher::schedule( gocb& goCBlock, uint16_t appid, uint32_t time_interval,
                                time_unit_t::type time_unit ) {
        // ensure that this dataset is not already being published
        if ( schedule_.find( goCBlock.dset.reference() ) != schedule_.end() ) {
          // this dataset is already being published
          return false;
        }

        boost::thread* publish_thread =
          new boost::thread(std::bind
                            (static_cast<void(publisher::*)(gocb*,uint16_t,uint32_t,time_unit_t::type)>(&publisher::publish_),
                             std::ref(*this), &goCBlock, appid, time_interval, time_unit));

        // track the publication
        publications.insert(std::make_pair(goCBlock.dset.reference(), goCBlock));

        // add a reference to the dataset in the schedule
        schedule_.insert( std::pair<string, boost::thread*>(goCBlock.dset.reference(), publish_thread) );

        return true;
      }

      bool publisher::un_schedule( string dataset_ref ) {
        auto entry = schedule_.find( dataset_ref );
        if ( entry == schedule_.end() ) {
          // the dataset was not found in the schedule
          return false;
        }

        // the dataset was scheduled, lets ask it to die politely
        (entry->second)->interrupt();

        // remove the entry from the schedule
        schedule_.erase( entry );

        return true;
      }

      bool publisher::un_schedule( gocb& goCBlock ) {
        return un_schedule(goCBlock.dset.reference());
      }

      void publisher::halt( void ) {
        for ( auto entry : schedule_ ) {
          if (not un_schedule(entry.first)) {
            std::cerr << "Publisher (WARNING): Failed to un-schedule a publication!" << std::endl;
          }
        }
      }

      void publisher::configure( string const& filename, uint32_t time_interval,
                                 time_unit_t::type time_unit ) {
        // process input CID file
        import_config(filename);

        // handle publications
        for (auto& pub : publications) {
          if (not schedule(pub.second, time_interval, time_unit)) {
            std::cout <<
              "Error: Failed to schedule dataset of GOOSE controlblock " <<
              pub.second.GoCBName() << std::endl;
          }
        }
      }

      void publisher::import_config( string const &filename ) {
        using namespace detail;
        using boost::property_tree::ptree;

        ptree pt;
        string ld_name, datSetName;

        // Read in XML config file
        read_xml(filename, pt);

        BOOST_FOREACH(ptree::value_type &v, pt.get_child("SCL.IED")) {
          if(v.first == "AccessPoint") {
            BOOST_FOREACH(ptree::value_type &v2, v.second.get_child("Server")) {
              if(v2.first == "LDevice") {
                ld_name = v2.second.get<string>("<xmlattr>.inst");
                BOOST_FOREACH(ptree::value_type &v3, v2.second.get_child("LN0")) {
                  if(v3.first == "GSEControl") {
                    uint8_t dstMac[] ={0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                    string GoCBName = v3.second.get<string>("<xmlattr>.name");
                    string datSetName = v3.second.get<string>("<xmlattr>.datSet");
                    string goID = v3.second.get<string>("<xmlattr>.appID");
                    gocb cblock(GoCBName, datSetName);
                    cblock.GoCBRef(build::gocb_reference(ld_name, "LLN0", GoCBName));
                    cblock.goEna(true);
                    cblock.goID(goID);
                    cblock.DatSet(build::dataset_reference((goID + ld_name), "LLN0", datSetName));
                    cblock.ConfRev(v3.second.get<uint32_t>("<xmlattr>.confRev"));
                    cblock.NdsCom(false);
                    cblock.DstAddress(dstMac);
                    /** We are currently inside a Goose Control Block definition, but now we need to
                        (still within the gocb) go out and get the dataset attribute types in order
                        to build the dataset for this gocb.  The types are nested in several element
                        references, so these must be obtained before the basic type can be determined.
                        The rough algorithm to go from Dataset->FCDA->BasicType is: Look up the matching
                        FCDA_doName in LD(FCDA_ldInst).LN(FCDA_prefix + FCDA_lnClass + FCDA_lnInst) and
                        store LN_lnType. Find matching LN_lnType in DataTypeTemplates.LNodeType section.
                        Find matching FCDA_doName in the list of DO's and store type. Search DOTypes section
                        where DOType_id=type. Find DOType.DA where DA_name=FCDA_daName. NOTE: If FCDA_daName
                        isn't specified, then we just add attributes for every DOType.DA. Here, DA_bType is the
                        BasicType we're looking for. However, if DA_bType is Enum or Struct, then DA_type
                        specifies the Enum or Struct definition. To get the Enum values or Struct basic types,
                        we must go one step further by finding the matching DA_type in EnumType section for
                        Enum or DAType section for Struct.

                        *******************************************************
                        * Example: Publish dataset "FLAIL" (daName specified) *
                        *******************************************************

                        <IED name="GOOSE_1" desc="R511 modifications and conformance enhancements" type="SEL_351" manufacturer="SEL">
                        <AccessPoint name="S1">
                        <Server>
                        <LDevice desc="Data Sets, BRCBs, URCBs" inst="CFG">
                        <LN0 lnType="LN0" lnClass="LLN0" inst="">
                        <DataSet name="FLAIL" desc="">
                        <FCDA ldInst="ANN" prefix="SVT" lnClass="GGIO" lnInst="6" doName="Ind01" daName="stVal" fc="ST"/>
                        |            |             |            |            |              |
                        |------------|-------------|------------|            |              |
                        |                         |              |
                        <LDevice desc="Annunciators" inst="ANN">        V                         |              |
                        <LN lnType="GGIO_BS16" lnClass="GGIO" inst="6" prefix="SVT">            |              |
                        |                                                       |              |
                        |                                                       |              |
                        <DataTypeTemplates>     V                                                       |              |
                        <LNodeType id="GGIO_BS16" iedType="SEL_351" lnClass="GGIO">                   |              |
                        <DO name="Ind01" type="SPS_0"/>                             <---------------|              |
                        |                                                                 |
                        |                                                                 |
                        V                                                                 |
                        <DOType id="SPS_0" cdc="SPS">                                                                |
                        <DA name="stVal" bType="BOOLEAN" dchg="true" fc="ST"/>      <------------------------------|




                        *************************************************************
                        * Example 2: Publish dataset "DSet01" (no daName specified) *
                        *************************************************************

                        <IED name="Goose451_1" desc="Conformance enhancements" type="SEL_451" manufacturer="SEL">
                        <AccessPoint name="S1">
                        <Server>
                        <LDevice desc="Data Sets, BRCBs, URCBs" inst="CFG">
                        <LN0 lnType="LN0" lnClass="LLN0" inst="">
                        <DataSet name="DSet01" desc="Metered Values">
                        <FCDA ldInst="MET" prefix="MET" lnClass="MMXU" lnInst="1" doName="TotW" fc="MX" />
                        |            |             |            |            |
                        |------------|-------------|------------|            |
                        |                         |
                        <LDevice desc="Metering" inst="MET">            V                         |
                        <LN lnType="MMXU1" lnClass="MMXU" inst="1" prefix="MET">                |
                        |                                                          |
                        |                                                          |
                        <DataTypeTemplates>  V                                                          |
                        <LNodeType id="MMXU1" iedType="SEL_451" lnClass="MMXU">                       |
                        <DO name="TotW" type="MV_0" />                         <--------------------|
                        |
                        |
                        V                       |----------------------------------|
                        <DOType id="MV_0" cdc="MV">                      |                                  |
                        <DA name="instMag" bType="Struct" type="AnalogValue_0" fc="MX"/>                  |
                        <DA name="mag" bType="Struct" type="AnalogValue_0" dchg="true" fc="MX" />         |
                        <DA name="q" bType="Quality" qchg="true" fc="MX" />                               |
                        <DA name="t" bType="Timestamp" fc="MX" />                                         |
                        <DA name="units" bType="Struct" type="Units_0" fc="CF" />                         |
                        <DA name="db" bType="INT32U" fc="CF" />  |                                        |
                        |                                        |
                        |                                        |
                        |--------------------------)----------------------------------------|
                        |                          |
                        V                          |
                        <DAType id="AnalogValue_0">                |
                        <BDA name="f" bType="FLOAT32" />         |
                        |
                        |----------------------------|
                        V
                        <DAType id="Units_0">
                        <BDA name="unit" bType="Enum" type="SIUnit_2" />
                        <BDA name="multiplier" bType="Enum" type="multiplier" />

                    */
                    BOOST_FOREACH(ptree::value_type &v4, v2.second.get_child("LN0")) {
                      if(v4.first == "DataSet" && v4.second.get<string>("<xmlattr>.name") == datSetName) {
                        string ldInst, prefix, lnClass, lnInst, doName, daName, lnType, doType, subDoType, basicType;
                        BOOST_FOREACH(ptree::value_type &v5, v4.second) {
                          if(v5.first == "FCDA") {
                            // get ldInst, prefix, lnClass, lnInst, doName, and daName
                            ldInst = v5.second.get<string>("<xmlattr>.ldInst");
                            prefix = v5.second.get<string>("<xmlattr>.prefix");
                            lnClass = v5.second.get<string>("<xmlattr>.lnClass");
                            lnInst = v5.second.get<string>("<xmlattr>.lnInst");
                            doName = v5.second.get<string>("<xmlattr>.doName");
                            daName = v5.second.get("<xmlattr>.daName", "");
                            // find corresponding LD and LN
                            BOOST_FOREACH(ptree::value_type &v6, v.second.get_child("Server")) {
                              if(v6.first == "LDevice" && v6.second.get<string>("<xmlattr>.inst") == ldInst) {
                                // find lnType
                                BOOST_FOREACH(ptree::value_type &v7, v6.second) {
                                  if(v7.first == "LN" && v7.second.get<string>("<xmlattr>.prefix") == prefix
                                     && v7.second.get<string>("<xmlattr>.lnClass") == lnClass
                                     && v7.second.get<string>("<xmlattr>.inst") == lnInst) {
                                    lnType = v7.second.get<string>("<xmlattr>.lnType");
                                    break;
                                  }
                                }
                                break;
                              }
                            }
                            // find LNodeType
                            BOOST_FOREACH(ptree::value_type &v8, pt.get_child("SCL.DataTypeTemplates")) {
                              if(v8.first == "LNodeType" && v8.second.get<string>("<xmlattr>.id") == lnType) {
                                // find doType
                                BOOST_FOREACH(ptree::value_type &v9, v8.second) {
                                  if(v9.first == "DO" && v9.second.get<string>("<xmlattr>.name") == doName) {
                                    doType = v9.second.get<string>("<xmlattr>.type");
                                    break;
                                  }
                                }
                                break;
                              }
                            }
                            // find corresponding DOType
                            BOOST_FOREACH(ptree::value_type &v10, pt.get_child("SCL.DataTypeTemplates")) {
                              if(v10.first == "DOType" && v10.second.get<string>("<xmlattr>.id") == doType) {
                                // find DA and basicType
                                BOOST_FOREACH(ptree::value_type &v11, v10.second) {
                                  // DOType contains DA's
                                  if(v11.first == "DA") {
                                    basicType = v11.second.get<string>("<xmlattr>.bType");
                                    string daN = v11.second.get<string>("<xmlattr>.name");
                                    std::cout<<datSetName<<" : "<<lnClass<<" : "<<doName<<" : (DO)"<<doType<<" : "<<daN<<std::endl;
                                    // if daName was specified, we want to search for the matching DA
                                    if(daName != "") {
                                      if(v11.second.get<string>("<xmlattr>.name") == daName) {
                                        // add the attribute
                                        if (attr_map.find(basicType) != attr_map.end()) {
                                          attr_map[basicType](&cblock.dset);
                                        } else {
                                          std::cout << "SCL WARNING: Requested publication of unsupported type!" << std::endl;
                                        }
                                        break;
                                      }
                                      else {
                                        continue;
                                      }
                                    }
                                    // if daName isn't specified, we want all the DA's
                                    else {
                                      //add the attribute
                                      if (attr_map.find(basicType) != attr_map.end()) {
                                        attr_map[basicType](&cblock.dset);
                                      } else {
                                        std::cout << "SCL WARNING: Requested publication of unsupported type!" << std::endl;
                                      }
                                      continue;
                                    }
                                  }
                                  // DOType contains SDO's (Sub Data Objects) which each reference other DOTypes
                                  else if(v11.first == "SDO") {
                                    subDoType = v11.second.get<string>("<xmlattr>.type");
                                    string daN = v11.second.get<string>("<xmlattr>.name");
                                    BOOST_FOREACH(ptree::value_type &v12, pt.get_child("SCL.DataTypeTemplates")) {
                                      if(v12.first == "DOType" && v12.second.get<string>("<xmlattr>.id") == subDoType) {
                                        // find DA and basicType
                                        BOOST_FOREACH(ptree::value_type &v13, v12.second) {
                                          // Here, DOType should contain DA's (there are no Sub-Sub Data Objects)
                                          // We assume a daName wouldn't be specified for a SDO, so we get all the DA's
                                          if(v13.first == "DA") {
                                            basicType = v13.second.get<string>("<xmlattr>.bType");
                                            string daN2 = v13.second.get<string>("<xmlattr>.name");
                                            std::cout<<datSetName<<" : "<<lnClass<<" : "<<doName<<" : (DO)"<<doType<<" : (SDO)"<<subDoType<<" : "<<daN<<" : "<<daN2<<std::endl;

                                            // add the attribute
                                            if (attr_map.find(basicType) != attr_map.end()) {
                                              attr_map[basicType](&cblock.dset);
                                            } else {
                                              std::cout << "SCL WARNING: Requested publication of unsupported type!" << std::endl;
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }

                                }
                                break;
                              }
                            }
                          }
                        }
                      }
                    }
                    publications.insert( std::pair<string, gocb>(cblock.dset.reference(), cblock) );
                  }
                }
              }
            }
          }
        }
      }

      void publisher::publish_( gocb* goCBlock, uint32_t time_interval, time_unit_t::type time_unit ) {

        deadline_timer timer(io_);

        while (1) {
          switch (time_unit) {
          case time_unit_t::SECONDS:
            // set timer wait time
            timer.expires_from_now(boost::posix_time::seconds(time_interval));
            break;
          case time_unit_t::MINUTES:
            // set timer wait time
            timer.expires_from_now(boost::posix_time::minutes(time_interval));
            break;
          case time_unit_t::HOURS:
            // set timer wait time
            timer.expires_from_now(boost::posix_time::hours(time_interval));
            break;
          default:
            /* handle error here */
            break;
          };

          // wait for the timer to expire, then publish the dataset
          timer.wait();
          publish( *goCBlock );

          // check to see if an interruption has been requested before processing
          // another publication
          boost::this_thread::interruption_point();
        }
      }

      void publisher::publish_( gocb* goCBlock, uint16_t appid, uint32_t time_interval,
                                time_unit_t::type time_unit ) {
        deadline_timer timer(io_);

        while (1) {
          switch (time_unit) {
          case time_unit_t::SECONDS:
            // set timer wait time
            timer.expires_from_now(boost::posix_time::seconds(time_interval));
            break;
          case time_unit_t::MINUTES:
            // set timer wait time
            timer.expires_from_now(boost::posix_time::minutes(time_interval));
            break;
          case time_unit_t::HOURS:
            // set timer wait time
            timer.expires_from_now(boost::posix_time::hours(time_interval));
            break;
          default:
            /* handle error here */
            break;
          };

          // wait for the timer to expire, then publish the dataset
          timer.wait();
          publish( *goCBlock, appid );

          // check to see if an interruption has been requested before processing
          // another publication
          boost::this_thread::interruption_point();
        }
      }

      void publisher::low_level_send_(uint8_t* buffer, size_t size, uint8_t* dest_mac_addr) {
        // NOTE: basic ethernet, no VLAN support for now

        // send buffer is the size of the GOOSE packet plus 2 mac addresses and
        // a packet type

        /*target address*/
        struct sockaddr_ll socket_address;

        /*buffer for ethernet frame*/
        uint8_t* eth_frame_buffer = (uint8_t*)malloc(ETH_FRAME_LEN);

        /*pointer to ethenet header*/
        uint8_t* etherhead = eth_frame_buffer;

        /*userdata in ethernet frame*/
        uint8_t* data = eth_frame_buffer + 14;

        /*another pointer to ethernet header*/
        struct ethhdr *eh = (struct ethhdr *)etherhead;

        /*prepare sockaddr_ll*/

        /*RAW communication*/
        socket_address.sll_family = PF_PACKET;

        /* carrying a GOOSE message */
        socket_address.sll_protocol = htons(GOOSE_ETHER_TYPE);

        /*index of the network device*/
        socket_address.sll_ifindex  = iface_index_;

        /*ARP hardware identifier is ethernet*/
        socket_address.sll_hatype  = ARPHRD_ETHER;

        /*target is another host*/
        socket_address.sll_pkttype  = PACKET_OTHERHOST;

        /*address length*/
        socket_address.sll_halen = ETH_ALEN;

        /*set the frame header*/
        memcpy((void*)eth_frame_buffer, (void*)dest_mac_addr, ETH_ALEN);
        memcpy((void*)(eth_frame_buffer+ETH_ALEN), (void*)&iface_addr_[0], ETH_ALEN);
        eh->h_proto = htons(GOOSE_ETHER_TYPE);

        /*add GOOSE packet into the frame*/
        if ( (size + 14) < ETH_FRAME_LEN ) {
          std::copy(buffer, buffer+size, data);

          /*send the packet*/
          sendto(raw_socket_, eth_frame_buffer, size+14, 0,
                 (struct sockaddr*)&socket_address, sizeof(socket_address));
        }
      }

      int publisher::get_interface_index_(const char *interface_name) {
        struct ifreq ifr;
        int fd;

        memset(&ifr, 0, sizeof(ifr));

        // setup ifr for ioctl
        strncpy (ifr.ifr_name, interface_name, sizeof(ifr.ifr_name) - 1);
        ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';

        // get index
        if (ioctl(raw_socket_, SIOCGIFINDEX, &ifr) == -1) {
          return (-1);
        }

        return ifr.ifr_ifindex;
      }

      vector<uint8_t> publisher::get_interface_address_(const char *interface_name) {
        struct ifreq ifr;
        vector<uint8_t> hwaddr;

        memset(&ifr, 0, sizeof(ifr));

        // setup ifr for ioctl
        strncpy (ifr.ifr_name, interface_name, sizeof(ifr.ifr_name) - 1);
        ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';

        if (ioctl(raw_socket_, SIOCGIFHWADDR, &ifr) == -1) {
          return hwaddr;
        }

        // copy and return the interface address
        hwaddr.resize(6);
        std::copy(ifr.ifr_hwaddr.sa_data, ifr.ifr_hwaddr.sa_data+6, &hwaddr[0]);

        return hwaddr;
      }

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_devices
