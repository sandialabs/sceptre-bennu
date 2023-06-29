/**
   @brief This file contains the data structures and definitions that make the
   Modbus application layer
*/
#ifndef __MODBUS_APPLICATION_LAYER_HPP__
#define __MODBUS_APPLICATION_LAYER_HPP__

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include <boost/fusion/container/map.hpp>
#include <boost/fusion/include/map.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/signals2.hpp>

#include "bennu/devices/modules/comms/modbus/protocol/application-callbacks.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/constants.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/function-codes.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/mbap-header.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/objects.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/types.hpp"

namespace bennu {
namespace comms {
namespace modbus {
namespace application {

    using boost::signals2::signal;
    namespace fusion = boost::fusion;

    class layer
    {
    public:
        layer(callback_map_t cbmap);
        ~layer() {}

        // master station api
        error_code_t::type read_coils(uint16_t start_address, uint16_t quantity, std::vector<bool>& values);
        error_code_t::type read_discrete_inputs(uint16_t start_address, uint16_t quantity, std::vector<bool>& values);
        error_code_t::type read_holding_registers(uint16_t start_address, uint16_t quantity, std::vector<uint16_t>& values);
        error_code_t::type read_input_registers(uint16_t start_address, uint16_t quantity, std::vector<uint16_t>& values);

        error_code_t::type write_coil(uint16_t address, bool value);
        error_code_t::type write_register(uint16_t address, uint16_t value);
        error_code_t::type write_coils(uint16_t start_address, std::vector<bool> const& values);
        error_code_t::type write_registers(uint16_t start_address, std::vector<uint16_t> const& values);

        // signals
        signal<void (uint8_t*, size_t)> data_send_signal;
        signal<void (uint8_t*, size_t)> awaiting_data_signal;

        // slots
        void handle_data_receive(uint8_t* rx_data, size_t size);

        // application function callback map
        callback_map_t callbacks;

        void set_unit_id(uint8_t unitId)
        {
            unit_id_ = unitId;
        }

    private:
        // request handler map
        typedef std::function<void (std::vector<uint8_t> const& request, std::vector<uint8_t>& response)> request_handler_fn_t;
        std::map<function_code_t::type, request_handler_fn_t> request_handler_map_;

        // next transaction-id to be used
        uint16_t transaction_id_;

        // optional unit identifier; defaults to 0x00
        uint8_t unit_id_;
    };

    /**
     Request_Handler
     Template parameter Function_Code should be one of the Function Code's
     defined in types.hpp
    */
    template<typename Function_Code>
    struct Request_Handler
    {
        typedef void result_type;
        void operator()(
            std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm);
    };

    // forward declarations for specializations of Request_Handler
    template<>
    void Request_Handler<WRITE_SINGLE_COIL>::operator() (
        std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm);

    template<>
    void Request_Handler<WRITE_SINGLE_REG>::operator() (
        std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm);

    template<>
    void Request_Handler<WRITE_MULTI_COIL>::operator() (
        std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm);

    template<>
    void Request_Handler<WRITE_MULTI_REG>::operator() (
        std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm);


} // namespace application
} // namespace modbus
} // namespace comms
} // namespace bennu


namespace detail
{
    using namespace bennu::comms::modbus;

    template <typename REGISTER_T>
    void serialize(std::vector<typename REGISTER_T::value_type> const& values, std::vector<uint8_t>& loc)
    {
        for ( auto value : values )
        {
            loc.push_back(static_cast<uint8_t>(value >> 8));
            loc.push_back(static_cast<uint8_t>(value));
        }
    }

    template <typename REGISTER_T>
    void deserialize(uint8_t const* buffer, uint8_t data_offset, uint16_t quantity,
        std::vector<typename REGISTER_T::value_type>& loc_values);



    template <typename REGISTER_T>
    void deserialize(std::vector<uint8_t> const& buffer, uint8_t data_offset, uint16_t quantity,
        std::vector<typename REGISTER_T::value_type>& loc_values)
    {
        deserialize<REGISTER_T>(&buffer[0], data_offset, quantity, loc_values);
    }

} // namespace detail

#endif /* __MODBUS_APPLICATION_LAYER_HPP__ */
