#ifndef __MODBUS_FUNCTION_CODES_HPP__
#define __MODBUS_FUNCTION_CODES_HPP__

#include <cstdint>

#include "bennu/devices/modules/comms/modbus/protocol/types.hpp"

namespace bennu {
namespace comms {
namespace modbus {

    /* Function Code Definitions can be found in the MODBUS Manual, page 22.
       The listing below shows the function codes supported by different Modicon controllers.
       Codes are listed in decimal. Y indicates that the function is supported. N indicates
       that it is not supported.

       Code    Name                        384 484 584 884 M84 984
       -----------------------------------------------------------
       01      Read Coil Status            Y   Y   Y   Y   Y   Y
       02      Read Input Status           Y   Y   Y   Y   Y   Y
       03      Read Holding Registers      Y   Y   Y   Y   Y   Y
       04      Read Input Registers        Y   Y   Y   Y   Y   Y
       05      Force Single Coil           Y   Y   Y   Y   Y   Y
       06      Preset Single Register      Y   Y   Y   Y   Y   Y
       07      Read Exception Status       Y   Y   Y   Y   Y   Y
       08      Diagnostics                 (see Chapter 3)
       09      Program 484                 N   Y   N   N   N   N
       10      Poll 484                    N   Y   N   N   N   N
       11      Fetch Comm. Event Ctr.      Y   N   Y   N   N   Y
       12      Fetch Comm. Event Log       Y   N   Y   N   N   Y
       13      Program Controller          Y   N   Y   N   N   Y
       14      Poll Controller             Y   N   Y   N   N   Y
       15      Force Multiple Coils        Y   Y   Y   Y   Y   Y
       16      Preset Multiple Registers   Y   Y   Y   Y   Y   Y
       17      Report Slave ID             Y   Y   Y   Y   Y   Y
       18      Program 884/M84             N   N   N   Y   Y   N
       19      Reset Comm. Link            N   N   N   Y   Y   N
       20      Read General Reference      N   N   Y   N   N   Y
       21      Write General Reference     N   N   Y   N   N   Y
    */

    //Supported protocol-defined function codes
    struct function_code_t
    {
        enum type
        {
            READ_COILS            =   0x01,
            READ_DISCRETE_INPUTS  =   0x02,
            READ_HOLDING_REGS     =   0x03,
            READ_INPUT_REGS       =   0x04,
            WRITE_SINGLE_COIL     =   0x05,
            WRITE_SINGLE_REG      =   0x06,
            WRITE_MULTI_COIL      =   0x0F,
            WRITE_MULTI_REG       =   0x10
        };
    };

    // Function Codes
    struct READ_COILS
    {
        typedef Coil register_type;
        enum function_code { func_code = function_code_t::READ_COILS };
    };

    struct READ_DISCRETE_INPUTS
    {
        typedef Discrete_Input register_type;
        enum function_code { func_code = function_code_t::READ_DISCRETE_INPUTS };
    };

    struct READ_HOLDING_REGS
    {
        typedef Holding_Register register_type;
        enum function_code { func_code = function_code_t::READ_HOLDING_REGS };
    };

    struct READ_INPUT_REGS
    {
        typedef Input_Register register_type;
        enum function_code { func_code = function_code_t::READ_INPUT_REGS };
    };

    struct WRITE_SINGLE_COIL
    {
        typedef Coil register_type;
        enum function_code { func_code = function_code_t::WRITE_SINGLE_COIL };
    };

    struct WRITE_SINGLE_REG
    {
        typedef Holding_Register register_type;
        enum function_code { func_code = function_code_t::WRITE_SINGLE_REG };
    };

    struct WRITE_MULTI_COIL
    {
        typedef Coil register_type;
        enum function_code { func_code = function_code_t::WRITE_MULTI_COIL };
    };

    struct WRITE_MULTI_REG
    {
        typedef Holding_Register register_type;
        enum function_code { func_code = function_code_t::WRITE_MULTI_REG };
    };

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif /* __MODBUS_FUNCTION_CODES_HPP__ */
