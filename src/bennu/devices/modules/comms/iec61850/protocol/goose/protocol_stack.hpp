/**
   @brief This file contains the API for the IEC61850 GOOSE protocol_stack object

   The IEC61850 protocol_stack object will provide access to the IEC61850 GOOSE
   protocol and allow any program to use its API to speak IEC61850 GOOSE
*/
#ifndef __IEC61850_PROTOCOL_STACK__
#define __IEC61850_PROTOCOL_STACK__

/* stl includes */
#include <cstdint>
#include <functional>
#include <vector>

/* boost includes */
#include <boost/signals2.hpp>

#include "bennu/devices/modules/comms/iec61850/protocol/goose/application-layer.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/session-options.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/object-reference-builder.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {

      using std::vector;
      using std::uint8_t;

      using boost::signals2::signal;

      class protocol_stack {
      public:
        protocol_stack();
        protocol_stack( struct session_opts& sopts );

    /* signals */
    signal<void ( uint8_t*, size_t )> data_receive_signal;

    /* signal handlers */
    void handle_data_send( uint8_t* tx_buffer, size_t size, uint8_t* dst_addr );

      public:
    /* protocol stack layers */
    application::layer app_layer;

      protected:
    low_level_interface_fn_t receive_fn_;   /**< physical interface rx callback */
    low_level_interface_fn_t transmit_fn_;  /**< physical interface tx callback */
      };

      typedef protocol_stack session;

    } /*namespace goose*/

  } /*namespace iec61850*/

} /*namespace ccss_protocols*/

#endif /*__IEC61850_PROTOCOL_STACK__*/
