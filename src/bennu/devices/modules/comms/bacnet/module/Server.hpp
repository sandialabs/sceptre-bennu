#ifndef BENNU_FIELDDEVICE_COMMS_BACNET_SERVER_HPP
#define BENNU_FIELDDEVICE_COMMS_BACNET_SERVER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/comms/base/CommsModule.hpp"
#include "bennu/utility/DirectLoggable.hpp"

namespace bennu {
namespace comms {
namespace bacnet {

enum PointType
{
    eInput,
    eOutput
};

class Server : public CommsModule, public utility::DirectLoggable, public std::enable_shared_from_this<Server>
{
public:
    Server(std::shared_ptr<field_device::DataManager> dm);

    void start(const std::string& endpoint, const std::uint32_t& address);

    void run();

    void update();

    bool addBinaryInput(const uint16_t address, const std::string& tag);

    bool addBinaryOutput(const uint16_t address, const std::string& tag);

    bool addAnalogInput(const uint16_t address, const std::string& tag);

    bool addAnalogOutput(const uint16_t address, const std::string& tag);

    void writeBinary(uint16_t address, bool value);

    void writeAnalog(uint16_t address, float value);

private:
    std::uint16_t mInstance;                            // BACnet device instance of local RTU
    std::shared_ptr<std::thread> pServerThread;			// Server thread
    std::shared_ptr<std::thread> pUpdateThread;			// Data sync update thread
    std::map<uint16_t, std::pair<std::string, PointType>> mBinaryPoints;
    std::map<uint16_t, std::pair<std::string, PointType>> mAnalogPoints;
    std::ostringstream mLogStream;                      // Logging output stream
};

} // namespace bacnet
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_BACNET_SERVER_HPP
