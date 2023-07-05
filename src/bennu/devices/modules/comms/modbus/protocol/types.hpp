/**
   @brief This file contains types used for template metaprogramming relevant
   to the types and values defined in the modbus specification
 */
#ifndef __MODBUS_TYPES_HPP__
#define __MODBUS_TYPES_HPP__

/* stl includes */
#include <cstdint>

#include "bennu/devices/modules/comms/modbus/protocol/constants.hpp"

namespace bennu {
namespace comms {
namespace modbus {

    using std::uint8_t;
    using std::uint16_t;

    /* Register Types

       These structs provide unique data types for template meta-programming.
       The "value_type" typedef in each struct defines what type is used to
       store the register data (not flags or other meta-data) for this register type.
    */
    struct Coil
    {
        typedef bool value_type;
        enum { min_read_quantity = MB_MIN_READ_QTY_COILS };
        enum { max_read_quantity = MB_MAX_READ_QTY_COILS };
    };

    struct Discrete_Input
    {
        typedef bool value_type;
        enum { min_read_quantity = MB_MIN_READ_QTY_DISCRETES };
        enum { max_read_quantity = MB_MAX_READ_QTY_DISCRETES };
    };

    struct Holding_Register {
        typedef uint16_t value_type;
        enum { min_read_quantity = MB_MIN_READ_QTY_HOLDING_REGS };
        enum { max_read_quantity = MB_MAX_READ_QTY_HOLDING_REGS };
    };

    struct Input_Register
    {
        typedef uint16_t value_type;
        enum { min_read_quantity = MB_MIN_READ_QTY_INPUT_REGS };
        enum { max_read_quantity = MB_MAX_READ_QTY_INPUT_REGS };
    };

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif /* __MODBUS_TYPES_HPP__ */
