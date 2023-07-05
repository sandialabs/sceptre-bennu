#ifndef BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_SERVER_HPP
#define BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_SERVER_HPP

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
#include "TcpChannel.hpp"
#include "SerialChannel.hpp"


namespace bennu {
namespace comms {
namespace modbus {

class Server : public CommsModule, public utility::DirectLoggable, public std::enable_shared_from_this<Server>
{
public:
    struct ScaledValue
    {
        std::pair<double, double> mRange;
        double mSlope;
        double mIntercept;
    };

public:
    Server(std::shared_ptr<field_device::DataManager> dm);

    ~Server();

    void start(std::string endpoint);

    bool addCoil(const std::uint16_t address, const std::string& tag);

    bool addDiscreteInput(const std::uint16_t address, const std::string& tag);

    bool addHoldingRegister(const std::uint16_t address, const std::string& tag, const std::pair<double, double>& range);

    bool addInputRegister(const std::uint16_t address, const std::string& tag, const std::pair<double, double>& range);

    void accept(const std::string& endpoint);

    error_code_t::type readCoils(std::uint16_t startAddress, std::uint16_t size, std::vector<bool>& values);
    error_code_t::type writeCoils(std::uint16_t startAddress, std::uint16_t size, const std::vector<bool>& value);
    error_code_t::type readDiscreteInputs(std::uint16_t startAddress, std::uint16_t size, std::vector<bool>& values);
    error_code_t::type readHoldingRegisters(std::uint16_t startAddress, std::uint16_t size, std::vector<std::uint16_t>& values);
    error_code_t::type writeHoldingRegisters(std::uint16_t startAddress, std::uint16_t size, const std::vector<std::uint16_t>& value);
    error_code_t::type readInputRegisters(std::uint16_t startAddress, std::uint16_t size, std::vector<std::uint16_t>& values);

protected:
    void acceptConnectionHandler(const boost::system::error_code& error);

    void setupConnection(const boost::system::error_code& error);

    void run(std::string endpoint);

private:
    boost::asio::io_service mIOService;
    std::shared_ptr<Channel> mChannel;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> mAcceptor;
    std::string mEndpoint;
    std::string mAddress;
    unsigned short mPort;
    std::shared_ptr<protocol_stack> mProtocolStack;
    std::shared_ptr<std::thread> mOutstationThread;
    std::map<std::uint16_t, std::string> mCoils;
    std::map<std::uint16_t, std::string> mDiscreteInputs;
    std::map<std::uint16_t, std::string> mHoldingRegisters;
    std::map<std::uint16_t, std::string> mInputRegisters;
    const double c16bitScale = 65535.0;
    std::map<std::uint16_t, ScaledValue> mScaledValues;
    std::vector<std::shared_ptr<Channel>> mConnections;
    Server(const Server&);
    Server& operator =(const Server&);

};

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_SERVER_HPP
