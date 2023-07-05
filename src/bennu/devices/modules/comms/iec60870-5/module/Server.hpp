#ifndef BENNU_FIELDDEVICE_COMMS_IEC60870_5_SERVER_HPP
#define BENNU_FIELDDEVICE_COMMS_IEC60870_5_SERVER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/comms/base/CommsModule.hpp"
#include "bennu/devices/modules/comms/iec60870-5/protocol/src/inc/api/cs104_slave.h"
#include "bennu/utility/DirectLoggable.hpp"

namespace bennu {
namespace comms {
namespace iec60870 {

/*
    *  IEC 60870-5-104 protocol provides the length of one APDU packet up to 255 bytes
    *  (include start character and length identification), so the maximum length of
    *  ASDU is 253. The length of APDU includes four octets of control field and ASDU,
    *  so the maximum length of ASDU is 249. This provision limits a APDU packet to
    *  send up to 121 normalized measured values without quality descriptor or 243
    *  single-point information. If the number of information collected by the
    *  sub-station RTU or monitoring system exceed above limit, we have to divide one
    *  APDU into more to send.
    *    - https://www.researchgate.net/publication/271304385_Realization_of_IEC_60870-5-104_Protocol_in_DTU
*/
#define MAX_ASDU_PAYLOAD_SIZE 240

enum PointType
{
    eInput,
    eOutput
};

class Server : public CommsModule, public utility::DirectLoggable, public std::enable_shared_from_this<Server>
{
public:
    Server(std::shared_ptr<field_device::DataManager> dm);

    void start(const std::string& endpoint, std::shared_ptr<Server> server, const uint32_t rPollRate);

    void reversePoll();

    bool addBinaryInput(const uint16_t address, const std::string& tag);

    bool addBinaryOutput(const uint16_t address, const std::string& tag);

    bool addAnalogInput(const uint16_t address, const std::string& tag);

    bool addAnalogOutput(const uint16_t address, const std::string& tag);

    void writeBinary(uint16_t address, bool value);

    void writeAnalog(uint16_t address, float value);

    // IEC60870-5-104 message callback handlers
    static void rawMessageHandler(void* parameter, IMasterConnection con, uint8_t* msg, int msgSize, bool sent);
    static bool interrogationHandler(void* parameter, IMasterConnection connection, CS101_ASDU asdu, uint8_t qoi);
    static bool asduHandler(void* parameter, IMasterConnection connection, CS101_ASDU asdu);
    static bool connectionRequestHandler(void* parameter, const char* ipAddress);
    static void connectionEventHandler(void* parameter, IMasterConnection con, CS104_PeerConnectionEvent event);

private:
    bool mConnected;
    uint32_t mReversePollRate;                              // Server reverse-poll rate
    IMasterConnection mConnection;                          // 104 master connection
    std::shared_ptr<std::thread> pServerPollThread;			// Server reverse-poll thread
    std::map<uint16_t, std::pair<std::string, PointType>> mBinaryPoints;
    std::map<uint16_t, std::pair<std::string, PointType>> mAnalogPoints;

};

// Static shared_ptr to server instance so it can be used inside static 104 callback handlers
static std::shared_ptr<Server> gServer;

} // namespace iec60870
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_IEC60870_5_SERVER_HPP
