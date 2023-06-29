/**
   @brief This file contains the modbus application layer function callback
   type definitions
*/
#ifndef __MODBUS_APPLICATION_CALLBACKS_HPP__
#define __MODBUS_APPLICATION_CALLBACKS_HPP__

#include <cstdint>
#include <functional>
#include <vector>

#include <boost/fusion/container/map.hpp>
#include <boost/fusion/include/map.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>

#include "bennu/devices/modules/comms/modbus/protocol/error-codes.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/function-codes.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/types.hpp"

namespace bennu {
namespace comms {
namespace modbus {
namespace application {

    using std::function;
    namespace fusion = boost::fusion;

    typedef function <error_code_t::type (uint16_t start_addr, uint16_t read_quantity, std::vector<typename Coil::value_type>& values)> read_coils_fn_t;
    typedef function <error_code_t::type (uint16_t start_addr, uint16_t read_quantity, std::vector<typename Discrete_Input::value_type>& values)> read_discrete_inputs_fn_t;
    typedef function <error_code_t::type (uint16_t start_addr, uint16_t read_quantity, std::vector<typename Holding_Register::value_type>& values)> read_holding_regs_fn_t;
    typedef function <error_code_t::type (uint16_t start_addr, uint16_t read_quantity, std::vector<typename Input_Register::value_type>& values)> read_input_regs_fn_t;
    typedef function <error_code_t::type (uint16_t start_addr, uint16_t write_quantity, std::vector<typename Coil::value_type> const& values)> write_single_coil_fn_t;
    typedef function <error_code_t::type (uint16_t start_addr, uint16_t write_quantity, std::vector<typename Holding_Register::value_type> const& value)> write_single_reg_fn_t;
    typedef function <error_code_t::type (uint16_t start_addr, uint16_t write_quantity, std::vector<typename Coil::value_type> const& values)> write_multiple_coils_fn_t;
    typedef function <error_code_t::type (uint16_t start_addr, uint16_t write_quantity, std::vector<typename Holding_Register::value_type> const& values)> write_multiple_regs_fn_t;

    typedef fusion::map<
    fusion::pair<READ_COILS, read_coils_fn_t>,
    fusion::pair<READ_DISCRETE_INPUTS, read_discrete_inputs_fn_t>,
    fusion::pair<READ_HOLDING_REGS, read_holding_regs_fn_t>,
    fusion::pair<READ_INPUT_REGS, read_input_regs_fn_t>,
    fusion::pair<WRITE_SINGLE_COIL, write_single_coil_fn_t>,
    fusion::pair<WRITE_SINGLE_REG, write_single_reg_fn_t>,
    fusion::pair<WRITE_MULTI_COIL, write_multiple_coils_fn_t>,
    fusion::pair<WRITE_MULTI_REG, write_multiple_regs_fn_t>
    > callback_map_t;

} // namespace application
} // namespace modbus
} // namespace comms
} // namespace bennu

#endif /* __MODBUS_APPLICATION_CALLBACKS_HPP__ */
