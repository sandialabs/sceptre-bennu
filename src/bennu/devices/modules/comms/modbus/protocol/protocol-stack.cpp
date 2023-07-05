/* stl includes */
#include <functional>

#include "bennu/devices/modules/comms/modbus/protocol/protocol-stack.hpp"

using namespace bennu::comms::modbus;

protocol_stack::protocol_stack(struct session_opts& sopts) :
    data_receive_signal(),
    app_layer(sopts.callbacks),
    transmit_fn_(sopts.transmit_fn),
    receive_fn_(sopts.receive_fn)
{
    // signal connections for receiving data
    data_receive_signal.connect(
        std::bind( &application::layer::handle_data_receive, std::ref(app_layer), std::placeholders::_1, std::placeholders::_2) );

    // signal connections for sending data
    app_layer.data_send_signal.connect(
        std::bind( &protocol_stack::handle_data_send, std::ref(*this), std::placeholders::_1, std::placeholders::_2) );

    // signal connections for awaiting data
    app_layer.awaiting_data_signal.connect(
        std::bind( &protocol_stack::handle_awaiting_data, std::ref(*this), std::placeholders::_1, std::placeholders::_2) );
}

void protocol_stack::handle_data_send( uint8_t* tx_data, size_t size )
{
    transmit_fn_(tx_data, size);
}

void protocol_stack::handle_awaiting_data( uint8_t* rx_data, size_t max_size )
{
    receive_fn_(rx_data, max_size);
}
