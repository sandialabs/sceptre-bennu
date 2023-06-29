/**
   @brief An IEC61850 GOOSE publisher outstation
*/
#ifndef __IEC61850_GOOSE_PUBLISHER_HPP__
#define __IEC61850_GOOSE_PUBLISHER_HPP__

/* stl includes */
#include <cstdint>
#include <map>
#include <string>

/* boost includes */
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/utility.hpp>

/* posix includes */
#include <sys/socket.h>

#include "bennu/devices/modules/comms/iec61850/device/attribute-map.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/protocol-stack.hpp"

namespace ccss_devices {
  namespace iec61850 {
    namespace goose {

      using std::map;
      using std::string;
      using std::uint16_t;
      using std::uint32_t;

      using boost::asio::io_service;

      using namespace ccss_protocols::iec61850;
      using namespace ccss_protocols::iec61850::goose;

      /** @brief Meta-type to specify a time unit when scheduling a dataset
          to be published on a schedule*/
      struct time_unit_t {
        enum type {
          SECONDS,
          MINUTES,
          HOURS
        };
      };

      /** @brief This class implements the GOOSE publisher functionality.

          With each dataset that is schedule() 'ed to be published, a new thread is
          spawned in order to perform the ongoing publication.

          No threads are spawned in performing a call to publish()
       */
      class publisher : virtual public attribute_map, private boost::noncopyable {
      public:
        /**@param iface_name Name of the network interface to publish data on*/
        publisher(const char* iface_name);

        /**@brief Publish a dataset immediately

           @param goCBlock GOOSE control block that manages a dataset to immediately publish
           @param appid Application ID associated with this GOOSE control-block
         */
        void publish( gocb& goCBlock, uint16_t appid = 0x0000 );

        /**@brief Schedule a dataset to be published every 'time_interval'

           The call to schedule will fail if a dataset with the same object
           reference has already been scheduled

           The default time_unit is seconds

           This function spawns new thread contexts in order to perform onging
           publications.

           @param goCBlock GOOSE control block managing a dataset to be scheduled
           @param time_interval number of time units to pass between each publish
           of the dataset
           @param time_unit type of time unit attributed to the time interval
           (e.g. seconds, minutes, hours)

           @return true if the dataset was scheduled, false on failure*/
        bool schedule( gocb& goCBlock, uint32_t time_interval,
                       time_unit_t::type time_unit = time_unit_t::SECONDS );

        /**@brief Same as the other schedule() function, but accepts an
           application ID.

           @param goCBlock GOOSE control block managing a dataset to be scheduled
           @param appid Application ID associated with the GOOSE control-block
           @param time_interval number of time units to pass between each publish
           of the dataset
           @param time_unit type of time unit attributed to the time interval
           (e.g. seconds, minutes, hours)

           @return true if the dataset was scheduled, false on failure*/
        bool schedule( gocb& goCBlock, uint16_t appid, uint32_t time_interval,
                       time_unit_t::type time_unit = time_unit_t::SECONDS );

        /** @brief Remove a dataset from the publishing schedule
            @param goCBlock GOOSE control block managing a dataset to be un-scheduled
            @return true if the dataset was removed from the schedule,
            false if the dataset was not found in the schedule*/
        bool un_schedule( gocb& goCBlock );

        /** @brief Same as un_schedule( gocb& ), but unschedules based on the dataset reference */
        bool un_schedule( string dataset_ref );

        /** @brief Halt all publications

            This function will end all publisher threads.  All publication and
            data will be purged.  After this call is made the user will need to
            rebuild all subscriptions and publications manually or by reloading
            an SCL CID configuration file.
        */
        void halt( void );

        /** @brief Loads CID configuration file and sets up all publications.

            The caveat here is that SCL configuration files don't appear to contain
            any information on what rate to publish consistent datasets.  This
            function will publish all datasets at a specified rate.  The interval
            is configurable.

            @param filename Name of the CID file to load
            @param time_interval number of time units to pass between each publish
            of the dataset
            @param time_unit type of time unit attributed to the time interval
            (e.g. seconds, minutes, hours)
        */
        void configure( string const& filename, uint32_t time_interval,
                        time_unit_t::type time_unit );

        /** @brief Loads CID configuration file

            @param filename Name of the CID file to load
        */
        void import_config( string const &filename );

      private:
        void publish_( gocb* goCBlock, uint32_t time_interval, time_unit_t::type time_unit );
        void publish_( gocb* goCBlock, uint16_t appid, uint32_t time_interval,
                       time_unit_t::type time_unit );

        void low_level_send_(uint8_t* buffer, size_t size, uint8_t* dest_mac_addr);

        int get_interface_index_(const char *interface_name);
        vector<uint8_t> get_interface_address_(const char *interface_name);

      public:
        // Listing of all scheduled publishing datasets
        map<string, gocb> publications;

      private:
        // Boost::Asio io_service (used for scheduling)
        io_service io_;

        // Ethernet communication variables
        int raw_socket_;
        int iface_index_;
        string iface_name_;
        vector<uint8_t> iface_addr_;

        // GOOSE session objects
        goose::session_opts goose_sopts_;
        goose::session*     goose_session_;

        // Mapping of datasets to their running contexts
        map<string, boost::thread*> schedule_;
      };

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_devices

#endif /* __IEC61850_GOOSE_PUBLISHER_HPP__ */
