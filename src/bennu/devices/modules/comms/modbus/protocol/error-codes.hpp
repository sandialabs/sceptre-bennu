#ifndef __MODBUS_ERROR_CODES_HPP__
#define __MODBUS_ERROR_CODES_HPP__

#include "bennu/devices/modules/comms/modbus/protocol/constants.hpp"

namespace bennu {
namespace comms {
namespace modbus {

    struct error_code_t
    {
        enum type
        {
            NO_ERROR                     = 0x00,

            // Modbus official error codes
            ILLEGAL_FUNCTION             = MB_ILLEGAL_FUNCTION,
            ILLEGAL_DATA_ADDRESS         = MB_ILLEGAL_DATA_ADDRESS,
            ILLEGAL_DATA_VALUE           = MB_ILLEGAL_DATA_VALUE,
            SLAVE_DEVICE_FAILURE         = MB_SLAVE_DEVICE_FAILURE,
            ACKNOWLEDGE                  = MB_ACKNOWLEDGE,
            SLAVE_DEVICE_BUSY            = MB_SLAVE_DEVICE_BUSY,
            MEMORY_PARITY_ERROR          = MB_MEMORY_PARITY_ERROR,
            GATEWAY_PATCH_UNAVAILABLE    = MB_GATEWAY_PATCH_UNAVAILABLE,
            GATEWAY_TARGET_DEVICE_FAILED = MB_GATEWAY_TARGET_DEVICE_FAILED,

            // User defined error codes (for internal use only)
            LENGTH_CONSTRAINT_FAILURE    = 0xF0,
            ERROR                        = 0xFF
        };
    };

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif /* __MODBUS_ERROR_CODES_HPP__ */
