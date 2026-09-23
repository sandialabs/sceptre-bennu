#ifndef BENNU_FIELDDEVICE_COMMS_IEC60870_5_CLIENTCONNECTION_HPP
#define BENNU_FIELDDEVICE_COMMS_IEC60870_5_CLIENTCONNECTION_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/iec60870-5/protocol/src/inc/api/cs104_connection.h"
#include "bennu/devices/modules/comms/iec60870-5/protocol/src/inc/api/cs101_information_objects.h"


namespace bennu {
namespace comms {
namespace iec60870 {

// The two command types the client issues, tracked so the async
// select-before-operate handshake can resend the correct command on execute.
enum PendingCommandType
{
    ePendingDouble,     // C_DC_NA_1 (binary output)
    ePendingSetpoint    // C_SE_NC_1 (analog output)
};

// A command awaiting its EXECUTE phase after a SELECT was sent. Held between the
// SELECT send and the arrival of the SELECT's positive ACT_CON.
struct PendingCommand
{
    PendingCommandType type;
    int value;          // Double point value (for ePendingDouble)
    float fValue;       // Setpoint value (for ePendingSetpoint)
};

class ClientConnection : public std::enable_shared_from_this<ClientConnection>
{
public:
    ClientConnection(const std::string& rtuEndpoint, bool sboEnabled = false);

    void start(std::shared_ptr<ClientConnection> clientConnection);

    void addBinary(const std::string& tag, const comms::RegisterDescriptor& rd)
    {
        mRegisters[tag] = rd;
        mBinaryAddressToTagMapping[rd.mRegisterAddress] = tag;
    }

    void addAnalog(const std::string& tag, const comms::RegisterDescriptor& rd)
    {
        mRegisters[tag] = rd;
        mAnalogAddressToTagMapping[rd.mRegisterAddress] = tag;
    }

    void updateBinary(const std::uint16_t address, const bool status)
    {
        auto iter = mBinaryAddressToTagMapping.find(address);
        if (iter != mBinaryAddressToTagMapping.end())
        {
            mRegisters[iter->second].mStatus = status;
        }
    }

    void updateAnalog(const std::uint16_t address, const double value)
    {
        auto iter = mAnalogAddressToTagMapping.find(address);
        if (iter != mAnalogAddressToTagMapping.end())
        {
            mRegisters[iter->second].mFloatValue = value;
        }
    }

    bool getRegisterDescriptorByTag(const std::string& tag, comms::RegisterDescriptor& rd)
    {
        auto iter = mRegisters.find(tag);
        if (iter != mRegisters.end())
        {
            rd = iter->second;
            return true;
        }
        return false;
    }

    StatusMessage readRegisterByTag(const std::string& tag, comms::RegisterDescriptor& rd);

    StatusMessage writeBinary(const std::string& tag, bool status);

    StatusMessage writeAnalog(const std::string& tag, double value);

    // IEC60870-5-104 message callback handlers
    static void rawMessageHandler(void* parameter, uint8_t* msg, int msgSize, bool sent);
    static void connectionHandler(void* parameter, CS104_Connection connection, CS104_ConnectionEvent event);
    static bool asduReceivedHandler(void* parameter, int address, CS101_ASDU asdu);

    static int convertBoolToDPValue(bool value);

private:
    // Send the EXECUTE phase (selectCommand=false) for a pending command at the
    // given IOA, if one exists. Called when a SELECT's positive ACT_CON arrives.
    void sendExecute(std::uint16_t address);

    bool mRunning;
    bool mSboEnabled;                           // Select-before-operate mode enabled
    std::string mRtuEndpoint;                   // IP/Port or DevName of remote RTU
    CS104_Connection mConnection;               // 104 connection object
    std::map<std::uint16_t, std::string> mBinaryAddressToTagMapping;
    std::map<std::uint16_t, std::string> mAnalogAddressToTagMapping;
    std::map<std::string, comms::RegisterDescriptor> mRegisters;
    std::map<std::uint16_t, PendingCommand> mPendingCommands;    // IOA -> command awaiting execute
    std::mutex mPendingMutex;                    // Guards mPendingCommands

};

// Static shared_ptr to client connection instance so it can be used inside static 104 callback handlers
static std::shared_ptr<ClientConnection> gClientConnection;

} // namespace iec60870
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_IEC60870_5_CLIENTCONNECTION_HPP
