#ifndef BENNU_FIELDDEVICE_COMMS_DNP3_SERVER_HPP
#define BENNU_FIELDDEVICE_COMMS_DNP3_SERVER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "opendnp3/DNP3Manager.h"

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/comms/base/CommsModule.hpp"
#include "bennu/utility/DirectLoggable.hpp"

namespace bennu {
namespace comms {
namespace dnp3 {

class ServerCommandHandler;

template <typename S, typename E>
struct Point {
    uint16_t    address {};
    std::string tag     {};

    S svariation {};
    E evariation {};

    bool sbo {};

    opendnp3::PointClass clazz;
    double deadband;
};

class Server : public CommsModule, public utility::DirectLoggable, public std::enable_shared_from_this<Server>
{
public:
    Server(std::shared_ptr<field_device::DataManager> dm);

    void init(const std::string& endpoint, const std::uint16_t& address);

    void start();

    void update();

    void configurePoints(opendnp3::DatabaseConfig& config);

    bool addBinaryInput
    (
        const uint16_t address,
        const std::string& tag,
        const std::string& sgvar,
        const std::string& egvar,
        const std::string& clazz
    );

    bool addBinaryOutput(const uint16_t address, const std::string& tag, const bool sbo);

    bool addAnalogInput
    (
        const uint16_t address,
        const std::string& tag,
        const std::string& sgvar,
        const std::string& egvar,
        const std::string& clazz,
        const double deadband
    );

    bool addAnalogOutput(const uint16_t address, const std::string& tag, const bool sbo);

    void writeBinary(uint16_t address, bool value);

    void writeAnalog(uint16_t address, float value);

    const Point<opendnp3::StaticBinaryVariation, opendnp3::EventBinaryVariation>*
    getBinaryPoint(const uint16_t address);

    const Point<opendnp3::StaticAnalogVariation, opendnp3::EventAnalogVariation>*
    getAnalogPoint(const uint16_t address);

private:
    std::shared_ptr<opendnp3::DNP3Manager> mManager;    // Outstation stack manager
    std::shared_ptr<ServerCommandHandler> pHandler;     // Pointer to command handler
    std::shared_ptr<opendnp3::IChannel> mChannel;       // TCPServer channel
    std::shared_ptr<opendnp3::IOutstation> mOutstation; // DNP3 outstation object
    std::uint16_t mAddress;                             // DNP3 address of local RTU
    std::shared_ptr<std::thread> mUpdateThread;

    // NOTE: this assumes inputs and outputs will not use the same addresses.
    std::map<uint16_t, Point<opendnp3::StaticBinaryVariation, opendnp3::EventBinaryVariation>> mBinaryPoints;
    std::map<uint16_t, Point<opendnp3::StaticAnalogVariation, opendnp3::EventAnalogVariation>> mAnalogPoints;

    std::ostringstream mLogStream; // Logging output stream
};

} // namespace dnp3
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_DNP3_SERVER_HPP
