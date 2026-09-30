#ifndef BENNU_FIELDDEVICE_COMMS_DNP3_SERVER_HPP
#define BENNU_FIELDDEVICE_COMMS_DNP3_SERVER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "opendnp3/DNP3Manager.h"
#include "opendnp3/outstation/DatabaseConfig.h"

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

    // NOTE: these must be initialized. configurePoints() copies them straight
    // into the opendnp3 DatabaseConfig, and an out-of-range PointClass causes
    // convert_to_event_class() to silently drop all events for the point.
    //
    // Defaults to Class0 (static data only, no events). Event generation is
    // opt-in per point via <class>Class1|Class2|Class3</class> in the XML
    // config, so masters that don't poll event classes never see the
    // EVENT_BUFFER_OVERFLOW IIN.
    opendnp3::PointClass clazz {opendnp3::PointClass::Class0};
    double deadband {0.0};
};

// Measurement points reported to a master (DNP3 groups 1/30).
using BinaryInputPoint = Point<opendnp3::StaticBinaryVariation,
                               opendnp3::EventBinaryVariation>;
using AnalogInputPoint = Point<opendnp3::StaticAnalogVariation,
                               opendnp3::EventAnalogVariation>;

// Control points written by a master. These are reported back as output status
// (DNP3 groups 10/40) so that masters performing a select/operate readback can
// verify the control point exists.
using BinaryOutputPoint = Point<opendnp3::StaticBinaryOutputStatusVariation,
                                opendnp3::EventBinaryOutputStatusVariation>;
using AnalogOutputPoint = Point<opendnp3::StaticAnalogOutputStatusVariation,
                                opendnp3::EventAnalogOutputStatusVariation>;

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

    bool addBinaryOutput(const uint16_t address, const std::string& tag, const bool sbo, const std::string& clazz);

    bool addAnalogInput
    (
        const uint16_t address,
        const std::string& tag,
        const std::string& sgvar,
        const std::string& egvar,
        const std::string& clazz,
        const double deadband
    );

    bool addAnalogOutput(const uint16_t address, const std::string& tag, const bool sbo, const std::string& clazz);

    // Per-type event buffer capacity. Must be called before init().
    void setEventBufferSize(const std::uint16_t size) { mEventBufferSize = size; }

    void writeBinary(uint16_t address, bool value);

    void writeAnalog(uint16_t address, float value);

    const BinaryOutputPoint* getBinaryPoint(const uint16_t address);

    const AnalogOutputPoint* getAnalogPoint(const uint16_t address);

private:
    std::shared_ptr<opendnp3::DNP3Manager> mManager;    // Outstation stack manager
    std::shared_ptr<ServerCommandHandler> pHandler;     // Pointer to command handler
    std::shared_ptr<opendnp3::IChannel> mChannel;       // TCPServer channel
    std::shared_ptr<opendnp3::IOutstation> mOutstation; // DNP3 outstation object
    std::uint16_t mAddress;                             // DNP3 address of local RTU
    std::shared_ptr<std::thread> mUpdateThread;
    std::uint16_t mEventBufferSize {100};               // Per-type event buffer capacity

    // Inputs and outputs are tracked separately so they can be registered in
    // the correct opendnp3 database tables (groups 1/30 vs 10/40). This also
    // means an input and an output may safely share the same DNP3 index.
    std::map<uint16_t, BinaryInputPoint> mBinaryPoints;
    std::map<uint16_t, AnalogInputPoint> mAnalogPoints;
    std::map<uint16_t, BinaryOutputPoint> mBinaryOutputPoints;
    std::map<uint16_t, AnalogOutputPoint> mAnalogOutputPoints;

    std::ostringstream mLogStream; // Logging output stream
};

} // namespace dnp3
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_DNP3_SERVER_HPP
