#ifndef BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CHANNEL_HPP
#define BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CHANNEL_HPP

#include <cstdint>
#include <memory>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <any>

#include <boost/asio.hpp>

#include "bennu/devices/modules/comms/base/CommsModule.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/error-codes.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/protocol-stack.hpp"
#include "bennu/utility/DirectLoggable.hpp"
#include "Channel.hpp"


namespace bennu {
namespace comms {
namespace modbus {

class TcpChannel : public Channel
{
public:
    TcpChannel(boost::asio::io_service& io_service);

    ~TcpChannel();

    void close();

    void manageSocket(std::shared_ptr<protocol_stack> protocolStack);

    void readHandler(const boost::system::error_code& error, size_t bytesTransferred);

    void transmit(std::uint8_t* buffer, size_t size);

    void reset(boost::asio::io_service& io_service);

    boost::asio::ip::tcp::socket getHandle();

    std::string getChannelType();

    boost::asio::ip::tcp::socket mSocket;

private:
    std::shared_ptr<protocol_stack> mProtocolStack;
    std::vector<std::uint8_t> mData;
    TcpChannel& operator =(const TcpChannel&);
};

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CHANNEL_HPP
