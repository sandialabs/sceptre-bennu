/**
   @brief An IEC61850 GOOSE outstation that can both subscribe to
   and publish GOOSE datasets
*/
#ifndef __IEC61850_GOOSE_OUTSTATION_HPP__
#define __IEC61850_GOOSE_OUTSTATION_HPP__

/* stl includes */
#include <functional>
#include <map>
#include <string>

#include "bennu/devices/modules/comms/iec61850/device/publisher.hpp"
#include "bennu/devices/modules/comms/iec61850/device/subscriber.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/protocol-stack.hpp"

namespace detail {
  using ccss_protocols::iec61850::goose::data_set;
  typedef std::function<void (data_set*)> add_attribute_t;
} // namespace detail

namespace ccss_devices {
  namespace iec61850 {
    namespace goose {
      using std::map;
      using std::string;
      using std::vector;

      using namespace ccss_protocols::iec61850;
      using namespace ccss_protocols::iec61850::goose;

      /** @brief This class combines the API from the publisher and subscriber
          into a single class instance.*/
      class outstation : public subscriber, public publisher {
      public:
        outstation(const char* iface) :
          publisher::publisher(iface), subscriber::subscriber(iface) {
        }

        /** @brief Halt the outstation

            This function will end all outstation threads.  All publication and
            subscription data will be purged.  After this call is made the user
            will need to rebuild all subscriptions and publications manually or
            by reloading an SCL CID configuration file.
         */
        void halt( void ) {subscriber::halt();publisher::halt();}

        /** @brief Loads CID configuration file and sets up all subscriptions and
            publications.

            The caveat here is that SCL configuration files don't appear to contain
            any information on what rate to publish consistent datasets.  This
            function will publish all datasets at a specified rate.  The interval
            is configurable.

            After this call is executed, the user has access to the publication and
            subscription lists through the public members 'publications' and
            'subscriptions'

            @param filename Name of the CID file to load
            @param subscription_cb Function callback that is called when a subscribed dataset is received
            @param time_interval number of time units to pass between each publish
            of the dataset
            @param time_unit type of time unit attributed to the time interval
            (e.g. seconds, minutes, hours)
        */
        inline void configure( string const& filename, subscription_callback_fn_t subscription_cb,
                        uint32_t time_interval = 5, time_unit_t::type time_unit = time_unit_t::SECONDS ) {
          subscriber::configure(filename, subscription_cb);
          publisher::configure(filename, time_interval, time_unit);
        }

        /** @brief Same as configure(), but does not perform any subscriptions. */
        inline void configure( string const& filename, uint32_t time_interval = 5,
                               time_unit_t::type time_unit = time_unit_t::SECONDS ) {
          publisher::configure(filename, time_interval, time_unit);
        }
      };

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_devices

#endif /* __IEC61850_GOOSE_OUTSTATION_HPP__ */
