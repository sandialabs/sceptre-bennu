/**
@brief Definition of the Modbus Application Header for TCP/IP comms
*/
#ifndef __MODBUS_MBAP_HEADER_HPP___
#define __MODBUS_MBAP_HEADER_HPP___

#include <cstdint>
#include <vector>

namespace bennu {
namespace comms {
namespace modbus {

/**
@brief Modbus TCP/IP header

The Modbus Application Protocol Header (MBAP) is defined in the specification
MODBUS Messaging on TCP/IP Implementation Guide V1.0b Section 3.1.3
*/
#pragma pack(push)
#pragma pack(1)

struct  mbap_header_t
{
    //The transaction ID is used for transaction pairing; the Modbus server must use the same
    //transaction ID specified by the client in the response.  NOTE: This field is big-endian
    //encoded.
    uint16_t transaction_id;

    //The protocol ID is used for intra-system multiplexing.  The Modbus protocol is identified
    //by the value 0x0000.  NOTE: This field is big-endian encoded.
    uint16_t protocol_id;

    //The length field is a byte count of all fields following it, including the unit identifier
    //byte and the data fields.  NOTE: This field is big-endian encoded.
    uint16_t length;

    //This field is used for intra-system routing purposes.  It is typically used to communicate
    //to a Modbus+ or a Modbus serial line slave through a gateway between an Ethernet TCP/IP
    //network and a Modbus serial line.  This field is set by the Modbus client in the request
    //and the server must use the same value specified by the client in its response.
    uint8_t unit_id;
};

#pragma pack(pop)

namespace mbap_header
{
    mbap_header_t parse( uint8_t const* pdu );
    std::vector<uint8_t> strip( uint8_t const* pdu, size_t size );
    void serialize( mbap_header_t const& hdr, std::vector<uint8_t>& loc );

    mbap_header_t build( uint8_t uid, uint16_t tid, uint16_t req_body_length);

} // namespace mbap_header

} // namespace modbus
} // namespace comms
} //namespace ccss_protocols

#endif /* __MODBUS_MBAP_HEADER_HPP___ */
