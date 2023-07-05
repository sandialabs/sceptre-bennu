/**
@brief This file container the API for the Modbus protocols stack object

The Modbus protocol_stack object will provide access to the Modbus protocol and allow
any program to use its API to speak Modbus
*/
#ifndef __MOBBUS_PROTOCOL_STACK__
#define	__MOBBUS_PROTOCOL_STACK__

#include <cstdint>

#include <boost/signals2.hpp>

#include "bennu/devices/modules/comms/modbus/protocol/objects.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/session-options.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/application-layer.hpp"

namespace bennu {
namespace comms {
namespace modbus {

    using boost::signals2::signal;

    class protocol_stack
    {
    public:
        protocol_stack(struct session_opts& sopts);

        // signals
        signal<void ( std::uint8_t*, size_t )> data_receive_signal;

        // slots
        void handle_data_send( std::uint8_t* tx_data, size_t size );
        void handle_awaiting_data( std::uint8_t* rx_data, size_t max_size );

    public:
        // protocol stack layers
        application::layer app_layer;

    protected:
        // physical interface tx/rx callbacks
        low_level_interface_fn_t transmit_fn_;
        low_level_interface_fn_t receive_fn_;

    };

    typedef protocol_stack session;

} // namespace modbus
} // namespace comms
} // namespace ccss_protocols

#endif /* __MOBBUS_PROTOCOL_STACK__ */
