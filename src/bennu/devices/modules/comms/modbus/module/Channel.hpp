#ifndef BENNU_FIELDDEVICE_COMMS_MODBUS_CHANNEL_HPP
#define BENNU_FIELDDEVICE_COMMS_MODBUS_CHANNEL_HPP

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


namespace bennu {
namespace comms {
namespace modbus {

class Channel
{
    public:
        Channel() = default;

        virtual ~Channel() = default;

        virtual void close() = 0;
        
        virtual void manageSocket(std::shared_ptr<protocol_stack> protocolStack) = 0;
        
        virtual void readHandler(const boost::system::error_code& error, size_t bytesTransferred) = 0;

        virtual void transmit(std::uint8_t* buffer, size_t size) = 0;

        virtual std::string getChannelType() = 0;
};

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_MODBUS_CHANNEL_HPP
