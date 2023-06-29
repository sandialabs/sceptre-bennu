#include <arpa/inet.h>
#include <algorithm>
#include <iostream>

#include "bennu/devices/modules/comms/modbus/protocol/application-layer.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/types.hpp"

using namespace bennu::comms::modbus;

namespace detail {
template <>
void serialize<Coil>(
        std::vector<typename Coil::value_type> const& values, std::vector<uint8_t>& loc)
{
    uint8_t packed_byte = 0x00;

    int i = 0;
    for ( auto value : values )
    {
        if ( value ) // coil is on
        {
            packed_byte |= static_cast<uint8_t>(0x01 << (i % 8));
        }
        if ( (i + 1) % 8 == 0 && (i != 0) )
        {
            loc.push_back( packed_byte );
            packed_byte = 0x00;
        }

        ++i;
    }

    if ( (values.size() % 8) != 0 )
    {
        loc.push_back(packed_byte);
    }
}

template <>
void serialize<Discrete_Input>(
        std::vector<typename Coil::value_type> const& values, std::vector<uint8_t>& loc)
{
    uint8_t packed_byte = 0x00;

    int i = 0;
    for ( auto value : values )
    {
        if ( value ) // discrete input is on
        {
            packed_byte |= static_cast<uint8_t>(0x01 << (i % 8));
        }

        if ( (i + 1) % 8 == 0 && (i != 0) )
        {
            loc.push_back( packed_byte );
            packed_byte = 0x00;
        }

        ++i;
    }

    if ( (values.size() % 8) != 0 )
    {
        loc.push_back(packed_byte);
    }
}


template <>
void deserialize<Coil>(
        uint8_t const* buffer, uint8_t data_offset, uint16_t quantity, std::vector<typename Coil::value_type>& loc_values)
{
    uint8_t packed_byte = 0x00;

    for (uint16_t i=0; i < quantity; ++i)
    {
        if (i % 8 == 0)
        {
            packed_byte = buffer[data_offset + (i / 8)];
        }

        if ((packed_byte & (0x01 << (i % 8))) != 0)
        {
            loc_values.push_back(true);
        }
        else
        {
            loc_values.push_back(false);
        }
    }
}

template <>
void deserialize<Discrete_Input>(
        uint8_t const* buffer, uint8_t data_offset, uint16_t quantity, std::vector<typename Discrete_Input::value_type>& loc_values)
{
    uint8_t packed_byte = 0x00;

    for (uint16_t i=0; i < quantity; ++i)
    {
        if (i % 8 == 0)
        {
            packed_byte = buffer[data_offset + (i / 8)];
        }

        if ((packed_byte & (0x01 << (i % 8))) != 0)
        {
            loc_values.push_back(true);
        }
        else
        {
            loc_values.push_back(false);
        }
    }
}

template <>
void deserialize<Holding_Register>(
        uint8_t const* buffer, uint8_t data_offset, uint16_t quantity, std::vector<typename Holding_Register::value_type>& loc_values)
{
    typename Holding_Register::value_type packed_short;
    for (uint16_t i = 0; i < quantity; ++i)
    {
        packed_short = ntohs(*reinterpret_cast<typename Holding_Register::value_type const*>( &buffer[data_offset + (2 * i)] ));
        loc_values.push_back( packed_short );
    }
}

template <>
void deserialize<Input_Register>(
        uint8_t const* buffer, uint8_t data_offset, uint16_t quantity, std::vector<typename Input_Register::value_type>& loc_values)
{
    typename Input_Register::value_type packed_short;
    for (uint16_t i = 0; i < quantity; ++i)
    {
        packed_short = ntohs(*reinterpret_cast<typename Input_Register::value_type const*>( &buffer[data_offset + (2 * i)] ));
        loc_values.push_back( packed_short );
    }
}

} // namespace detail


mbap_header_t mbap_header::parse( uint8_t const* pdu )
{
    mbap_header_t hdr =
    {
        ntohs(*reinterpret_cast<uint16_t const*>(&pdu[0])),
        ntohs(*reinterpret_cast<uint16_t const*>(&pdu[2])),
        ntohs(*reinterpret_cast<uint16_t const*>(&pdu[4])),
        pdu[6]
    };
    return hdr;
}

std::vector<uint8_t> mbap_header::strip( uint8_t const* pdu, size_t size )
{
    std::vector<uint8_t> stripped(size - sizeof(mbap_header_t));
    std::copy(&pdu[0] + sizeof(mbap_header_t),
            &pdu[0] + size, &stripped[0]);

    return stripped;
}

void mbap_header::serialize( mbap_header_t const& hdr, std::vector<uint8_t>& loc )
{
    uint8_t const* start = reinterpret_cast<uint8_t const*>(&hdr);

    if (loc.size() < sizeof(mbap_header_t))
    {
        return;
    }

    unsigned int i;
    uint8_t tmp_byte = *start;
    for (i = 0; i < sizeof(mbap_header_t)-1; ++i)
    {
        if (i % 2 == 0)
        {
            tmp_byte = *(start+i);
        }
        else
        {
            loc[i-1] = *(start+i);
            loc[i] = tmp_byte;
        }
    }

    loc[i] = *(start+i);
}

mbap_header_t mbap_header::build(uint8_t uid, uint16_t tid, uint16_t req_body_length)
{
    mbap_header_t hdr;
    hdr.transaction_id = tid;
    hdr.protocol_id = 0x0000;
    hdr.length = static_cast<uint16_t>(req_body_length + 1);
    hdr.unit_id = uid;

    return hdr;
}

// -----------------------------------
// layer Implementation
// -----------------------------------
application::layer::layer(callback_map_t cbmap) :
    data_send_signal(),
    awaiting_data_signal(),
    callbacks(cbmap),
    request_handler_map_(),
    transaction_id_(3)
{
    request_handler_map_.insert(std::make_pair( function_code_t::READ_COILS,
                                                std::bind(Request_Handler<READ_COILS>(), std::placeholders::_1, std::placeholders::_2, std::ref(callbacks))));

    request_handler_map_.insert(std::make_pair(function_code_t::READ_DISCRETE_INPUTS,
                                               std::bind(Request_Handler<READ_DISCRETE_INPUTS>(), std::placeholders::_1, std::placeholders::_2, std::ref(callbacks))));

    request_handler_map_.insert(std::make_pair(function_code_t::READ_HOLDING_REGS,
                                               std::bind(Request_Handler<READ_HOLDING_REGS>(), std::placeholders::_1, std::placeholders::_2, std::ref(callbacks))));

    request_handler_map_.insert(std::make_pair(function_code_t::READ_INPUT_REGS,
                                               std::bind(Request_Handler<READ_INPUT_REGS>(), std::placeholders::_1, std::placeholders::_2, std::ref(callbacks))));

    request_handler_map_.insert(std::make_pair(function_code_t::WRITE_SINGLE_COIL,
                                               std::bind(Request_Handler<WRITE_SINGLE_COIL>(), std::placeholders::_1, std::placeholders::_2, std::ref(callbacks))));

    request_handler_map_.insert(std::make_pair(function_code_t::WRITE_SINGLE_REG,
                                               std::bind(Request_Handler<WRITE_SINGLE_REG>(), std::placeholders::_1, std::placeholders::_2, std::ref(callbacks))));

    request_handler_map_.insert(std::make_pair(function_code_t::WRITE_MULTI_COIL,
                                               std::bind(Request_Handler<WRITE_MULTI_COIL>(), std::placeholders::_1, std::placeholders::_2, std::ref(callbacks))));

    request_handler_map_.insert(std::make_pair(function_code_t::WRITE_MULTI_REG,
                                               std::bind(Request_Handler<WRITE_MULTI_REG>(), std::placeholders::_1, std::placeholders::_2, std::ref(callbacks))));
}

error_code_t::type application::layer::read_coils(uint16_t start_address, uint16_t quantity, std::vector<bool>& values)
{
    // intialize the pdu
    std::vector<uint8_t> pdu(sizeof(mbap_header_t));

    // create the pdu body
    pdu.push_back(READ_COILS::func_code);
    pdu.push_back(static_cast<uint8_t>(start_address >> 8)); // start addr HI
    pdu.push_back(static_cast<uint8_t>(start_address)); // start addr LO
    pdu.push_back(static_cast<uint8_t>(quantity >> 8)); // quantity HI
    pdu.push_back(static_cast<uint8_t>(quantity)); // quantity LO

    // add in the pdu header
    mbap_header_t hdr =
            mbap_header::build(unit_id_, transaction_id_, static_cast<uint16_t>(pdu.size() - sizeof(mbap_header_t)));
    transaction_id_++;

    std::vector<uint8_t> serialized_hdr(sizeof(mbap_header_t));
    mbap_header::serialize(hdr, serialized_hdr);

    std::copy(&serialized_hdr[0],
            &serialized_hdr[0] + sizeof(mbap_header_t), &pdu[0]);

    // send the request
    data_send_signal(&pdu[0], pdu.size());

    // await the response
    uint8_t response[100];
    awaiting_data_signal(response, 100);

    /** verify the response **/
    mbap_header_t response_hdr = mbap_header::parse(response);

    // ensure the received message meets the ModbusADU length constraints.
    if ( response_hdr.length > MB_MAX_ADU_LENGTH )
    {
        return error_code_t::LENGTH_CONSTRAINT_FAILURE;
    }

    // check the function code
    if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] != READ_COILS::func_code )
    {
        const uint8_t ERROR_OFFSET = 1;

        if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] == (READ_COILS::func_code + 0x80) )
        {
            // retrieve the error code
            return (static_cast<error_code_t::type>(response[ERROR_OFFSET+sizeof(mbap_header_t)]));
        }
        else
        {
            return error_code_t::ERROR;
        }
    }

    // extract the requested data
    uint8_t const DATA_OFFSET = 2 + sizeof(mbap_header_t);
    detail::deserialize<READ_COILS::register_type>(response, DATA_OFFSET, quantity, values);

    return error_code_t::NO_ERROR;
}


error_code_t::type application::layer::read_discrete_inputs(uint16_t start_address, uint16_t quantity, std::vector<bool>& values)
{
    // intialize the pdu
    std::vector<uint8_t> pdu(sizeof(mbap_header_t));

    // create the pdu body
    pdu.push_back(READ_DISCRETE_INPUTS::func_code);
    pdu.push_back(static_cast<uint8_t>(start_address >> 8)); // start addr HI
    pdu.push_back(static_cast<uint8_t>(start_address)); // start addr LO
    pdu.push_back(static_cast<uint8_t>(quantity >> 8)); // quantity HI
    pdu.push_back(static_cast<uint8_t>(quantity)); // quantity LO

    // add in the pdu header
    mbap_header_t hdr =
            mbap_header::build(unit_id_, transaction_id_, static_cast<uint16_t>(pdu.size() - sizeof(mbap_header_t)));
    transaction_id_++;

    std::vector<uint8_t> serialized_hdr(sizeof(mbap_header_t));
    mbap_header::serialize(hdr, serialized_hdr);

    std::copy(&serialized_hdr[0],
            &serialized_hdr[0] + sizeof(mbap_header_t), &pdu[0]);

    // send the request
    data_send_signal(&pdu[0], pdu.size());

    // await the response
    uint8_t response[100];
    awaiting_data_signal(response, 100);

    /** verify the response **/
    mbap_header_t response_hdr = mbap_header::parse(response);

    // ensure the received message meets the ModbusADU length constraints.
    if ( response_hdr.length > MB_MAX_ADU_LENGTH )
    {
        return error_code_t::LENGTH_CONSTRAINT_FAILURE;
    }

    // check the function code
    if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] != READ_DISCRETE_INPUTS::func_code )
    {
        const uint8_t ERROR_OFFSET = 1;

        if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] == (READ_DISCRETE_INPUTS::func_code + 0x80) )
        {
            // retrieve the error code
            return (static_cast<error_code_t::type>(response[ERROR_OFFSET+sizeof(mbap_header_t)]));
        }
        else
        {
            return error_code_t::ERROR;
        }
    }

    // extract the requested data
    uint8_t const DATA_OFFSET = 2 + sizeof(mbap_header_t);
    detail::deserialize<READ_DISCRETE_INPUTS::register_type>(response, DATA_OFFSET, quantity, values);

    return error_code_t::NO_ERROR;
}


error_code_t::type application::layer::read_holding_registers(uint16_t start_address, uint16_t quantity, std::vector<uint16_t>& values)
{
    // intialize the pdu
    std::vector<uint8_t> pdu(sizeof(mbap_header_t));

    // create the pdu body
    pdu.push_back(READ_HOLDING_REGS::func_code);
    pdu.push_back(static_cast<uint8_t>(start_address >> 8)); // start addr HI
    pdu.push_back(static_cast<uint8_t>(start_address)); // start addr LO
    pdu.push_back(static_cast<uint8_t>(quantity >> 8)); // quantity HI
    pdu.push_back(static_cast<uint8_t>(quantity)); // quantity LO

    // add in the pdu header
    mbap_header_t hdr =
            mbap_header::build(unit_id_, transaction_id_, static_cast<uint16_t>(pdu.size() - sizeof(mbap_header_t)));
    transaction_id_++;

    std::vector<uint8_t> serialized_hdr(sizeof(mbap_header_t));
    mbap_header::serialize(hdr, serialized_hdr);

    std::copy(&serialized_hdr[0],
            &serialized_hdr[0] + sizeof(mbap_header_t), &pdu[0]);

    // send the request
    data_send_signal(&pdu[0], pdu.size());

    // await the response
    uint8_t response[100];
    awaiting_data_signal(response, 100);

    /** verify the response **/
    mbap_header_t response_hdr = mbap_header::parse(response);

    // ensure the received message meets the ModbusADU length constraints.
    if ( response_hdr.length > MB_MAX_ADU_LENGTH )
    {
        return error_code_t::LENGTH_CONSTRAINT_FAILURE;
    }

    // check the function code
    if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] != READ_HOLDING_REGS::func_code )
    {
        const uint8_t ERROR_OFFSET = 1;

        if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] == (READ_HOLDING_REGS::func_code + 0x80) )
        {
            // retrieve the error code
            return (static_cast<error_code_t::type>(response[ERROR_OFFSET+sizeof(mbap_header_t)]));
        }
        else
        {
            return error_code_t::ERROR;
        }
    }

    // extract the requested data
    uint8_t const DATA_OFFSET = 2 + sizeof(mbap_header_t);
    detail::deserialize<READ_HOLDING_REGS::register_type>(response, DATA_OFFSET, quantity, values);

    return error_code_t::NO_ERROR;
}


error_code_t::type application::layer::read_input_registers(uint16_t start_address, uint16_t quantity, std::vector<uint16_t>& values)
{
    // intialize the pdu
    std::vector<uint8_t> pdu(sizeof(mbap_header_t));

    // create the pdu body
    pdu.push_back(READ_INPUT_REGS::func_code);
    pdu.push_back(static_cast<uint8_t>(start_address >> 8)); // start addr HI
    pdu.push_back(static_cast<uint8_t>(start_address)); // start addr LO
    pdu.push_back(static_cast<uint8_t>(quantity  >> 8)); // quantity HI
    pdu.push_back(static_cast<uint8_t>(quantity)); // quantity LO

    // add in the pdu header
    mbap_header_t hdr =
            mbap_header::build(unit_id_, transaction_id_, static_cast<uint16_t>(pdu.size() - sizeof(mbap_header_t)));
    transaction_id_++;

    std::vector<uint8_t> serialized_hdr(sizeof(mbap_header_t));
    mbap_header::serialize(hdr, serialized_hdr);

    std::copy(&serialized_hdr[0],
            &serialized_hdr[0] + sizeof(mbap_header_t), &pdu[0]);

    // send the request
    data_send_signal(&pdu[0], pdu.size());

    // await the response
    uint8_t response[100];
    awaiting_data_signal(response, 100);

    /** verify the response **/
    mbap_header_t response_hdr = mbap_header::parse(response);

    // ensure the received message meets the ModbusADU length constraints.
    if ( response_hdr.length > MB_MAX_ADU_LENGTH )
    {
        return error_code_t::LENGTH_CONSTRAINT_FAILURE;
    }

    // check the function code
    if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] != READ_INPUT_REGS::func_code )
    {
        const uint8_t ERROR_OFFSET = 1;

        if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] == (READ_INPUT_REGS::func_code + 0x80) )
        {
            // retrieve the error code
            return (static_cast<error_code_t::type>(response[ERROR_OFFSET+sizeof(mbap_header_t)]));
        }
        else
        {
            return error_code_t::ERROR;
        }
    }

    // extract requested data
    uint8_t const DATA_OFFSET = 2 + sizeof(mbap_header_t);
    detail::deserialize<READ_INPUT_REGS::register_type>(response, DATA_OFFSET, quantity, values);

    return error_code_t::NO_ERROR;
}

error_code_t::type application::layer::write_coil(uint16_t address, bool value)
{
    // intialize the pdu
    std::vector<uint8_t> pdu(sizeof(mbap_header_t));

    // create the pdu body
    pdu.push_back(WRITE_SINGLE_COIL::func_code);
    pdu.push_back(static_cast<uint8_t>(address >> 8)); // start addr HI
    pdu.push_back(static_cast<uint8_t>(address)); // start addr LO

    if (value)
    {
        pdu.push_back(0xFF); pdu.push_back(0x00);
    }
    else
    {
        pdu.push_back(0x00); pdu.push_back(0x00);
    }

    // add in the pdu header
    mbap_header_t hdr =
            mbap_header::build(unit_id_, transaction_id_, static_cast<uint16_t>(pdu.size() - sizeof(mbap_header_t)));
    transaction_id_++;

    std::vector<uint8_t> serialized_hdr(sizeof(mbap_header_t));
    mbap_header::serialize(hdr, serialized_hdr);

    std::copy(&serialized_hdr[0],
            &serialized_hdr[0] + sizeof(mbap_header_t), &pdu[0]);

    // send the request
    data_send_signal(&pdu[0], pdu.size());

    // await the response
    uint8_t response[100];
    awaiting_data_signal(response, 100);

    /** verify the response **/
    mbap_header_t response_hdr = mbap_header::parse(response);

    // ensure the received message meets the ModbusADU length constraints.
    if ( response_hdr.length > MB_MAX_ADU_LENGTH )
    {
        return error_code_t::LENGTH_CONSTRAINT_FAILURE;
    }

    // check the function code
    if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] != WRITE_SINGLE_COIL::func_code )
    {
        const uint8_t ERROR_OFFSET = 1;

        if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] == (WRITE_SINGLE_COIL::func_code + 0x80) )
        {
            // retrieve the error code
            return (static_cast<error_code_t::type>(response[ERROR_OFFSET+sizeof(mbap_header_t)]));
        }
        else
        {
            return error_code_t::ERROR;
        }
    }

    return error_code_t::NO_ERROR;
}


error_code_t::type application::layer::write_register(uint16_t address, uint16_t value)
{
    // intialize the pdu
    std::vector<uint8_t> pdu(sizeof(mbap_header_t));

    // create the pdu body
    pdu.push_back(WRITE_SINGLE_REG::func_code);
    pdu.push_back(static_cast<uint8_t>(address >> 8)); // start addr HI
    pdu.push_back(static_cast<uint8_t>(address)); // start addr LO

    pdu.push_back(static_cast<uint8_t>(value >> 8)); // value HI
    pdu.push_back(static_cast<uint8_t>(value)); // value LO

    // add in the pdu header
    mbap_header_t hdr =
            mbap_header::build(unit_id_, transaction_id_, static_cast<uint16_t>(pdu.size() - sizeof(mbap_header_t)));
    transaction_id_++;

    std::vector<uint8_t> serialized_hdr(sizeof(mbap_header_t));
    mbap_header::serialize(hdr, serialized_hdr);

    std::copy(&serialized_hdr[0],
            &serialized_hdr[0] + sizeof(mbap_header_t), &pdu[0]);

    // send the request
    data_send_signal(&pdu[0], pdu.size());

    // await the response
    uint8_t response[100];
    awaiting_data_signal(response, 100);

    /** verify the response **/
    mbap_header_t response_hdr = mbap_header::parse(response);

    // ensure the received message meets the ModbusADU length constraints.
    if ( response_hdr.length > MB_MAX_ADU_LENGTH )
    {
        return error_code_t::LENGTH_CONSTRAINT_FAILURE;
    }

    // check the function code
    if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] != WRITE_SINGLE_REG::func_code )
    {
        const uint8_t ERROR_OFFSET = 1;

        if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] == (WRITE_SINGLE_REG::func_code + 0x80) )
        {
            // retrieve the error code
            return (static_cast<error_code_t::type>(response[ERROR_OFFSET+sizeof(mbap_header_t)]));
        }
        else
        {
            return error_code_t::ERROR;
        }
    }

    return error_code_t::NO_ERROR;
}


error_code_t::type application::layer::write_coils(uint16_t start_address, std::vector<bool> const& values)
{
    // intialize the pdu
    std::vector<uint8_t> pdu(sizeof(mbap_header_t));

    // create the pdu body
    pdu.push_back(WRITE_MULTI_COIL::func_code);
    pdu.push_back(static_cast<uint8_t>(start_address >> 8)); // start addr HI
    pdu.push_back(static_cast<uint8_t>(start_address)); // start addr LO

    uint16_t quantity = static_cast<uint16_t>(values.size());
    pdu.push_back(static_cast<uint8_t>(quantity >> 8)); // quantity HI
    pdu.push_back(static_cast<uint8_t>(quantity)); // quantity LO

    std::vector<uint8_t> serialized_values;
    detail::serialize<WRITE_MULTI_COIL::register_type>(values, serialized_values);

    uint8_t byte_count = static_cast<uint8_t>(serialized_values.size());
    pdu.push_back(byte_count);

    for ( uint8_t byte : serialized_values )
    {
        pdu.push_back(byte);
    }

    // add in the pdu header
    mbap_header_t hdr =
            mbap_header::build(unit_id_, transaction_id_, static_cast<uint16_t>(pdu.size() - sizeof(mbap_header_t)));
    transaction_id_++;

    std::vector<uint8_t> serialized_hdr(sizeof(mbap_header_t));
    mbap_header::serialize(hdr, serialized_hdr);

    std::copy(&serialized_hdr[0],
            &serialized_hdr[0] + sizeof(mbap_header_t), &pdu[0]);

    // send the request
    data_send_signal(&pdu[0], pdu.size());

    // await the response
    uint8_t response[100];
    awaiting_data_signal(response, 100);

    /** verify the response **/
    mbap_header_t response_hdr = mbap_header::parse(response);

    // ensure the received message meets the ModbusADU length constraints.
    if ( response_hdr.length > MB_MAX_ADU_LENGTH )
    {
        return error_code_t::LENGTH_CONSTRAINT_FAILURE;
    }

    // check the function code
    if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] != WRITE_MULTI_COIL::func_code )
    {
        const uint8_t ERROR_OFFSET = 1;

        if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] == (WRITE_MULTI_COIL::func_code + 0x80) )
        {
            // retrieve the error code
            return (static_cast<error_code_t::type>(response[ERROR_OFFSET+sizeof(mbap_header_t)]));
        }
        else
        {
            return error_code_t::ERROR;
        }
    }

    return error_code_t::NO_ERROR;
}


error_code_t::type application::layer::write_registers(uint16_t start_address, std::vector<uint16_t> const& values)
{
    // intialize the pdu
    std::vector<uint8_t> pdu(sizeof(mbap_header_t));

    // create the pdu body
    pdu.push_back(WRITE_MULTI_REG::func_code);
    pdu.push_back(static_cast<uint8_t>(start_address >> 8)); // start addr HI
    pdu.push_back(static_cast<uint8_t>(start_address)); // start addr LO

    uint16_t quantity = static_cast<uint16_t>(values.size());
    pdu.push_back(static_cast<uint8_t>(quantity >> 8)); // quantity HI
    pdu.push_back(static_cast<uint8_t>(quantity)); // quantity LO

    std::vector<uint8_t> serialized_values;
    detail::serialize<WRITE_MULTI_REG::register_type>(values, serialized_values);

    uint8_t byte_count = static_cast<uint8_t>(serialized_values.size());
    pdu.push_back(byte_count);

    for ( uint8_t byte : serialized_values )
    {
        pdu.push_back(byte);
    }

    // add in the pdu header
    mbap_header_t hdr =
            mbap_header::build(unit_id_, transaction_id_, static_cast<uint16_t>(pdu.size() - sizeof(mbap_header_t)));
    transaction_id_++;

    std::vector<uint8_t> serialized_hdr(sizeof(mbap_header_t));
    mbap_header::serialize(hdr, serialized_hdr);

    std::copy(&serialized_hdr[0],
            &serialized_hdr[0] + sizeof(mbap_header_t), &pdu[0]);

    // send the request
    data_send_signal(&pdu[0], pdu.size());

    // await the response
    uint8_t response[100];
    awaiting_data_signal(response, 100);

    /** verify the response **/
    mbap_header_t response_hdr = mbap_header::parse(response);

    // ensure the received message meets the ModbusADU length constraints.
    if ( response_hdr.length > MB_MAX_ADU_LENGTH )
    {
        return error_code_t::LENGTH_CONSTRAINT_FAILURE;
    }

    // check the function code
    if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] != WRITE_MULTI_REG::func_code )
    {
        const uint8_t ERROR_OFFSET = 1;

        if ( response[MB_FUNC_CODE_OFFSET+sizeof(mbap_header_t)] == (WRITE_MULTI_REG::func_code + 0x80) )
        {
            // retrieve the error code
            return (static_cast<error_code_t::type>(response[ERROR_OFFSET+sizeof(mbap_header_t)]));
        }
        else
        {
            return error_code_t::ERROR;
        }
    }

    return error_code_t::NO_ERROR;
}

void application::layer::handle_data_receive(uint8_t* rx_data, size_t size)
{
    std::vector<uint8_t> response;
    response.assign(sizeof(mbap_header_t), 0x00);
    mbap_header_t response_hdr = mbap_header::parse(rx_data);

    //Send an error response if the received message does not meet ModbusADU length constraints.
    if ( size < MB_MIN_REQUEST_ADU_LENGTH || size > MB_MAX_ADU_LENGTH )
    {
        // Set lenth to the 3 bytes (unit ID byte, error code byte, and exception code byte)
        response_hdr.length = 3;
        mbap_header::serialize(response_hdr, response);

        //First byte of error response is 0x80, as no function code was received (this is not necessarily per Modbus specification).
        response.push_back(0x80);

        //Second byte of response is 0x04, indicating that an unrecoverable error occurred when processing the request.
        response.push_back(MB_SLAVE_DEVICE_FAILURE);

        // send the response
        data_send_signal(&response[0], response.size());

        return;
    }

    //////////////////////////////////////////////////
    // Process Modbus packet based on function code
    // strip the MBAP header
    std::vector<uint8_t> modbus_data = mbap_header::strip(rx_data, size);

    // get the function code handler
    std::map<function_code_t::type, request_handler_fn_t>::iterator request_handler =
            request_handler_map_.find(static_cast<function_code_t::type>(modbus_data[MB_FUNC_CODE_OFFSET]));

    if (request_handler != request_handler_map_.end())
    {
        // call the request handler
        request_handler->second(modbus_data, response);
    }
    //
    ///////////////////////////////////////////////////

    // add the MBAP header to the response
    response_hdr.length = static_cast<uint16_t>(response.size() - (sizeof(mbap_header_t)-1));
    mbap_header::serialize(response_hdr, response);

    // send the response
    data_send_signal(&response[0], response.size());
}


// --------------------------------
// Function code handlers
// --------------------------------

// matches function codes 0x01 - 0x04
template<typename Function_Code>
void application::Request_Handler<Function_Code>::operator()(
    std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm)
{
    //Is the request for a legal quantity of registers?
    uint16_t quantity = ntohs(*reinterpret_cast<uint16_t const*>( &request[MB_QTY_PDU_OFFSET] ));
    if (quantity < Function_Code::register_type::min_read_quantity ||
            quantity > Function_Code::register_type::max_read_quantity)
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_DATA_VALUE);

        return;
    }

    //Does the request overflow the address space?
    uint16_t start_address = ntohs(*reinterpret_cast<uint16_t const*>( &request[MB_START_ADDR_PDU_OFFSET] ));
    if ( static_cast<uint32_t>(start_address + quantity) > MB_MAX_ADDRESS )
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_DATA_ADDRESS);

        return;
    }

    //Get the coil values from the application (using a callback)
    std::vector<typename Function_Code::register_type::value_type> values;

    // If there is no callback associated with the function code then
    // create an error response. Otherwise, perform callback function
    // associated with the function code.
    if ( not fusion::at_key<Function_Code>(cm) )
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_FUNCTION);

        return;
    }
    else
    {
        error_code_t::type callback_return = fusion::at_key<Function_Code>(cm)
                (start_address, quantity, values);

        // If callback returns no error, send normal response.
        // Otherwise, send error response.
        if ( callback_return == error_code_t::NO_ERROR )
        {
            /////////////////////////////
            // Create the response PDU

            // 1. The first byte of a normal response is the function code from the request
            response.push_back(request[MB_FUNC_CODE_OFFSET]);

            // 2. The second byte in a normal response is the number of bytes in which the reg values are packed.
            //    We won't know that until serialize() is called.  We will put in zero for now.
            response.push_back(0x00);

            // 3. Pack the register values
            size_t preserializeSize = response.size();
            detail::serialize<typename Function_Code::register_type>(values, response);

            // 4. Update the number of bytes in which the reg values are packed.
            size_t num_bytes = (response.size() - preserializeSize);
            response[sizeof(mbap_header_t)+1] = static_cast<uint8_t>(num_bytes);

        }
        else
        {
            //First byte of an error response is the request function code plus 0x80.
            response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

            //Second byte of an error response is the exception code.
            response.push_back(callback_return);

        }
    }
}

template<>
void application::Request_Handler<WRITE_SINGLE_COIL>::operator()(
    std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm)
{
    // Does the request overflow the address space
    uint16_t start_address = ntohs(*reinterpret_cast<uint16_t const*>( &request[MB_START_ADDR_PDU_OFFSET] ));

    //Deserialize the write values
    uint16_t quantity = 1;
    std::vector<typename WRITE_SINGLE_COIL::register_type::value_type> values;

    uint8_t const DATA_OFFSET = 3;
    if ( request[DATA_OFFSET] == 0xFF )
    {
        values.push_back(true);
    }
    else
    {
        values.push_back(false);
    }

    //Write the values to the application (using a callback)
    // If there is no callback associated with the function code then
    // create an error response. Otherwise, perform callback function
    // associated with the function code.
    if ( not fusion::at_key<WRITE_SINGLE_COIL>(cm) )
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_FUNCTION);

        return;
    }
    else
    {
        //If this fails it would be an ILLEGAL_DATA_ADDRESS
        error_code_t::type callback_return = fusion::at_key<WRITE_SINGLE_COIL>(cm)
                (start_address, quantity, values);

        // If callback returns no error, send normal response.
        // Otherwise, send error response.
        if ( callback_return == error_code_t::NO_ERROR )
        {
            // Send a normal response (an echo of the request)
            response.resize( request.size() + response.size() );
            std::copy( &request[0], &request[0] + request.size(), &response[sizeof(mbap_header_t)]);
        }
        else
        {
            //First byte of an error response is the request function code plus 0x80.
            response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

            //Second byte of an error response is the exception code.
            response.push_back(callback_return);
        }
    }
}


template<>
void application::Request_Handler<WRITE_SINGLE_REG>::operator()(
    std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm)
{
    // Does the request overflow the address space
    uint16_t start_address = ntohs(*reinterpret_cast<uint16_t const*>( &request[MB_START_ADDR_PDU_OFFSET] ));

    //Deserialize the write values
    uint16_t quantity = 1;
    uint8_t const DATA_OFFSET = 3;

    std::vector<typename WRITE_SINGLE_REG::register_type::value_type> values;
    detail::deserialize<WRITE_SINGLE_REG::register_type>(request, DATA_OFFSET, quantity, values);

    //Write the values to the application (using a callback)
    // If there is no callback associated with the function code then
    // create an error response. Otherwise, perform callback function
    // associated with the function code.
    if ( not fusion::at_key<WRITE_SINGLE_REG>(cm) )
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_FUNCTION);

        return;
    }
    else
    {
        //If this fails it would be an ILLEGAL_DATA_ADDRESS
        error_code_t::type callback_return = fusion::at_key<WRITE_SINGLE_REG>(cm)
                (start_address, quantity, values);

        // If callback returns no error, send normal response.
        // Otherwise, send error response.
        if ( callback_return == error_code_t::NO_ERROR )
        {
            // Send a normal response (an echo of the request)
            response.resize( request.size() + response.size() );
            std::copy( &request[0], &request[0] + request.size(), &response[sizeof(mbap_header_t)]);
        }
        else
        {
            //First byte of an error response is the request function code plus 0x80.
            response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

            //Second byte of an error response is the exception code.
            response.push_back(callback_return);
        }
    }
}


template<>
void application::Request_Handler<WRITE_MULTI_COIL>::operator()(
    std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm)
{
    //Is the request for a legal quantity of registers?
    uint16_t quantity = ntohs(*reinterpret_cast<uint16_t const*>( &request[MB_QTY_PDU_OFFSET] ));
    if (quantity < MB_MIN_READ_QTY_COILS || quantity > MB_MAX_READ_QTY_COILS)
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_DATA_VALUE);

        return;
    }

    //Does the request overflow the address space?
    uint16_t start_address = ntohs(*reinterpret_cast<uint16_t const*>( &request[MB_START_ADDR_PDU_OFFSET] ));
    if ( static_cast<uint32_t>(start_address + quantity) > MB_MAX_ADDRESS )
    {
        // First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        // Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_DATA_VALUE);

        return;
    }

    // Determine how many bytes of coil data the request claims to hold.
    uint8_t number_bytes_request = request[5];

    // Calculate how many bytes of coil data should be in the request.
    uint8_t number_bytes_calculated = static_cast<uint8_t>(quantity / 8);
    if (quantity % 8 != 0)
    {
        number_bytes_calculated++;
    }

    // Does the claimed and calculated number of coil data bytes align
    if (number_bytes_request != number_bytes_calculated)
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_DATA_VALUE);

        return;
    }

    // Unpack the coil values
    uint8_t const DATA_OFFSET = 6;
    std::vector<typename WRITE_MULTI_COIL::register_type::value_type> values;
    detail::deserialize<WRITE_MULTI_COIL::register_type>(request, DATA_OFFSET, quantity, values);

    //Write the values to the application (using a callback)
    // If there is no callback associated with the function code then
    // create an error response. Otherwise, perform callback function
    // associated with the function code.
    if ( not fusion::at_key<WRITE_MULTI_COIL>(cm) )
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_FUNCTION);

        return;
    }
    else
    {
        //If this fails it would be an ILLEGAL_DATA_ADDRESS
        error_code_t::type callback_return = fusion::at_key<WRITE_MULTI_COIL>(cm)
                (start_address, quantity, values);

        // If callback returns no error, send normal response.
        // Otherwise, send error response.
        if ( callback_return == error_code_t::NO_ERROR )
        {

            //A normal response is simply an echo of the first five bytes of the request
            response.resize( response.size() + 5 );
            std::copy( &request[0], &request[0] + 5, &response[sizeof(mbap_header_t)]);

        }
        else
        {

            //First byte of an error response is the request function code plus 0x80.
            response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

            //Second byte of an error response is the exception code.
            response.push_back(callback_return);
        }
    }
}

template<>
void application::Request_Handler<WRITE_MULTI_REG>::operator()(
    std::vector<uint8_t> const& request, std::vector<uint8_t>& response, callback_map_t const& cm)
{
    //Is the request for a legal quantity of registers?
    uint16_t quantity = ntohs(*reinterpret_cast<uint16_t const*>( &request[MB_QTY_PDU_OFFSET] ));
    if (quantity < MB_MIN_READ_QTY_COILS || quantity > MB_MAX_READ_QTY_COILS)
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_DATA_VALUE);

        return;
    }

    //Does the request overflow the address space?
    uint16_t start_address = ntohs(*reinterpret_cast<uint16_t const*>( &request[MB_START_ADDR_PDU_OFFSET] ));
    if ( static_cast<uint32_t>(start_address + quantity) > MB_MAX_ADDRESS )
    {
        // First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        // Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_DATA_VALUE);

        return;
    }

    // Determine how many bytes of holding register data the request claims to hold.
    uint8_t number_bytes_request = request[5];

    //Calculate how many bytes of holding register data should be in the request.
    uint8_t number_bytes_calculated = static_cast<uint8_t>(quantity * sizeof(uint16_t));

    //Does claimed and calculated number of holding register data bytes align
    if (number_bytes_request != number_bytes_calculated)
    {
        // First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        // Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_DATA_VALUE);

        return;
    }

    uint8_t const DATA_OFFSET = 6;
    std::vector<typename WRITE_MULTI_REG::register_type::value_type> values;
    detail::deserialize<WRITE_MULTI_REG::register_type>(request, DATA_OFFSET, quantity, values);

    //Write the values to the application (using a callback)
    // If there is no callback associated with the function code then
    // create an error response. Otherwise, perform callback function
    // associated with the function code.
    if ( not fusion::at_key<WRITE_MULTI_REG>(cm) )
    {
        //First byte of an error response is the request function code plus 0x80.
        response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

        //Second byte of an error response is the exception code.
        response.push_back(MB_ILLEGAL_FUNCTION);

        return;
    }
    else
    {
        //If this fails it would be an ILLEGAL_DATA_ADDRESS
        error_code_t::type callback_return = fusion::at_key<WRITE_MULTI_REG>(cm)
                (start_address, quantity, values);

        // If callback returns no error, send normal response.
        // Otherwise, send error response.
        if ( callback_return == error_code_t::NO_ERROR )
        {

            //A normal response is simply an echo of the first five bytes of the request
            response.resize( response.size() + 5 );
            std::copy( &request[0], &request[0] + 5, &response[sizeof(mbap_header_t)]);

        }
        else
        {

            //First byte of an error response is the request function code plus 0x80.
            response.push_back(static_cast<uint8_t>(request[MB_FUNC_CODE_OFFSET] + 0x80));

            //Second byte of an error response is the exception code.
            response.push_back(callback_return);
        }
    }
}
