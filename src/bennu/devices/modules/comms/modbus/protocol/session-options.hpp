/**
   @brief This file contains the modbus session options data-structure definition
*/
#ifndef __MODBUS_SESSION_OPTIONS_HPP__
#define __MODBUS_SESSION_OPTIONS_HPP__

#include <cstdint>
#include <functional>

#include "bennu/devices/modules/comms/modbus/protocol/application-callbacks.hpp"

namespace bennu {
namespace comms {
namespace modbus {

    typedef std::function <void (uint8_t* buffer, size_t size)> low_level_interface_fn_t;

    struct session_opts
    {
        session_opts() :
            receive_fn(nullptr),
            transmit_fn(nullptr),
            callbacks()
        {

        }

        low_level_interface_fn_t receive_fn;
        low_level_interface_fn_t transmit_fn;

        application::callback_map_t callbacks;

    };

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif /* __MODBUS_SESSION_OPTIONS_HPP__ */
