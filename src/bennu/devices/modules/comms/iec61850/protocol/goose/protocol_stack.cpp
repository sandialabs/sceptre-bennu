/**
   @brief This file contains the implementation for the API for the IEC651850
   GOOSE protocol_stack object

   The IEC651850 GOOSE protocol_stack object will provide access to the IEC651850
   GOOSE protocol and allow any program to use its API to speak IEC651850 GOOSE
 */
/* stl includes */
#include <functional>

#include "bennu/devices/modules/comms/iec61850/protocol/goose/protocol_stack.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {

      using std::placeholders::_1;
      using std::placeholders::_2;
      using std::placeholders::_3;

      protocol_stack::protocol_stack()  {
        // signal connections for receiving data
    data_receive_signal.connect
      (std::bind(&application::layer::handle_data_receive, std::ref(app_layer), _1, _2));

    // signal connections for sending data
    app_layer.data_send_signal.connect
      (std::bind(&protocol_stack::handle_data_send, std::ref(*this), _1, _2, _3));
      }

      protocol_stack::protocol_stack( struct session_opts& sopts ) :
    data_receive_signal(), app_layer(sopts.update_dataset_callback),
        receive_fn_(sopts.receive_fn), transmit_fn_(sopts.transmit_fn) {

    // signal connections for receiving data
    data_receive_signal.connect
      (std::bind(&application::layer::handle_data_receive, std::ref(app_layer), _1, _2));

    // signal connections for sending data
    app_layer.data_send_signal.connect
      (std::bind(&protocol_stack::handle_data_send, std::ref(*this), _1, _2, _3));
      }

      void protocol_stack::handle_data_send( uint8_t* tx_buffer, size_t size, uint8_t* dst_addr ) {
    if ( transmit_fn_ )
      transmit_fn_( tx_buffer, size, dst_addr );
      }

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_protocols
