#include "Server.hpp"

#include <chrono>
#include <sstream>

#include "bennu/devices/modules/comms/iec60870-5/module/DataHandler.hpp"
#include "bennu/devices/modules/comms/iec60870-5/protocol/src/inc/api/cs104_slave.h"

namespace bennu {
namespace comms {
namespace iec60870 {

Server::Server(std::shared_ptr<field_device::DataManager> dm) :
    bennu::utility::DirectLoggable("iec60870-5-104-server"),
    mConnected(false)
{
    setDataManager(dm);
}

void Server::start(const std::string& endpoint, std::shared_ptr<Server> server, const uint32_t rPollRate)
{
    // Set server reverse-poll rate
    mReversePollRate = rPollRate;
    // Set static shared_ptr to server instance so it can be used inside static 104 callback handlers
    gServer = server;
    // If endpoint starts with tcp://, parse ip/port and use TCP
    std::size_t findResult = endpoint.find("tcp://");

    if (findResult != std::string::npos)
    {
        // Handle splitting out ip and port from endpoint
        std::string ipAndPort = endpoint.substr(findResult + 6);
        std::string ip = ipAndPort.substr(0, ipAndPort.find(":"));
        std::uint16_t port = static_cast<std::uint16_t>(stoi(ipAndPort.substr(ipAndPort.find(":") + 1)));

        // Create a new slave/server instance with default connection parameters and
        // default message queue size
        auto slave = CS104_Slave_create(1000, 1000);

        // Set parameters
        CS104_Slave_setLocalAddress(slave, ip.data());
        CS104_Slave_setLocalPort(slave, port);
        // Set mode to a single redundancy group
        // NOTE: library has to be compiled with CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP enabled (=1)
        CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);

        // Set the callback handler for the interrogation command
        CS104_Slave_setInterrogationHandler(slave, interrogationHandler, NULL);
        // Set handler for other message types
        CS104_Slave_setASDUHandler(slave, asduHandler, NULL);
        // Set handler to handle connection requests (optional)
        CS104_Slave_setConnectionRequestHandler(slave, connectionRequestHandler, NULL);
        // Set handler to track connection events (optional)
        CS104_Slave_setConnectionEventHandler(slave, connectionEventHandler, NULL);
        // Uncomment to log messages
        //CS104_Slave_setRawMessageHandler(slave, rawMessageHandler, NULL);

        // Start 104 slave thread
        std::cout << "starting slave: " << endpoint << std::endl;
        CS104_Slave_start(slave);

        if (CS104_Slave_isRunning(slave) == false)
        {
            logEvent("iec60870-5-104 server start", "error", "Starting server failed");
            CS104_Slave_destroy(slave);
        }
        else
        {
            // Start server reverse-polling thread
            pServerPollThread.reset(new std::thread(std::bind(&Server::reversePoll, this)));
        }

    }
    else
    {
        logEvent("iec60870-5-104 server start", "error", "Unknown endpoint protocol (" + endpoint + ")");
        return;
    }

    // Log data size
    std::ostringstream log_stream;
    log_stream << "Initialized IEC60870-5-104 server: " << endpoint;
    logEvent("iec60870-5-104 server start", "info", log_stream.str());
    std::cout << log_stream.str() << std::endl;
    fflush(stdout);
}

DoublePointValue Server::convertBoolToDPValue(bool status)
{
    if (status == 0)
        return IEC60870_DOUBLE_POINT_OFF;
    else
        return IEC60870_DOUBLE_POINT_ON;
}

DoublePointValue Server::convertIntToDPValue(int value)
{
    switch (value) 
    {
        case 0:
            return IEC60870_DOUBLE_POINT_INTERMEDIATE;
            break;
        case 1:
            return IEC60870_DOUBLE_POINT_OFF;
            break;
        case 2:
            return IEC60870_DOUBLE_POINT_ON;
            break;
        case 3:
            return IEC60870_DOUBLE_POINT_INDETERMINATE;
            break;
        default:
            return IEC60870_DOUBLE_POINT_INTERMEDIATE;
    }
}

/*
 * Send spontaneous monitor update on specific data point
 */
void Server::sendSpontaneousUpdate(IMasterConnection connection, int ioa, DoublePointValue status)
{
    CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);
    CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);
    struct sCP56Time2a currentTime;
    CP56Time2a_createFromMsTimestamp(&currentTime, Hal_getTimeInMs());
    InformationObject io = (InformationObject)DoublePointWithCP56Time2a_create(NULL, ioa, status, IEC60870_QUALITY_GOOD, &currentTime);
    CS101_ASDU_addInformationObject(newAsdu, io);
    InformationObject_destroy(io);
    IMasterConnection_sendASDU(connection, newAsdu);
    CS101_ASDU_destroy(newAsdu);
}

/*
 * Reverse poll loop that sends local bennu datastore data to
 * connected clients.
 */
void Server::reversePoll()
{
    while (1)
    {
        if (mConnected)
        {
            CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(mConnection);

            // Send binary values
            // kv = {<address>: {<tag>, eInput}}
            for (const auto &kv : gServer->mBinaryPoints)
            {
                const std::string tag = kv.second.first;
                sendSpontaneousUpdate(mConnection, kv.first, convertBoolToDPValue(gServer->mDataManager->getDataByTag<bool>(tag)));
            }

            // Send analog values
            CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);
            // kv = {<address>: {<tag>, eInput}}
            for (const auto &kv : gServer->mAnalogPoints)
            {
                const std::string tag = kv.second.first;
                if (CS101_ASDU_getPayloadSize(newAsdu) < MAX_ASDU_PAYLOAD_SIZE)
                {
                    if (gServer->mDataManager->hasTag(tag))
                    {
                        auto val = gServer->mDataManager->getDataByTag<double>(tag);
                        InformationObject io = (InformationObject)MeasuredValueShort_create(NULL, kv.first, val, IEC60870_QUALITY_GOOD);
                        CS101_ASDU_addInformationObject(newAsdu, io);
                        InformationObject_destroy(io);
                    }
                }
                else
                {
                    // Send current ASDU and create a new one for the remaining values
                    IMasterConnection_sendASDU(mConnection, newAsdu);
                    CS101_ASDU_destroy(newAsdu);
                    newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);
                    if (gServer->mDataManager->hasTag(tag))
                    {
                        auto val = gServer->mDataManager->getDataByTag<double>(tag);
                        InformationObject io = (InformationObject)MeasuredValueShort_create(NULL, kv.first, val, IEC60870_QUALITY_GOOD);
                        CS101_ASDU_addInformationObject(newAsdu, io);
                        InformationObject_destroy(io);
                    }
                }
            }
            IMasterConnection_sendASDU(mConnection, newAsdu);
            CS101_ASDU_destroy(newAsdu);
            std::this_thread::sleep_for(std::chrono::seconds(mReversePollRate));
        }
        else
        {
            // Wait until connected to client
            while (!mConnected) { std::this_thread::sleep_for(std::chrono::seconds(1)); }
        }
    }
}

/*
* Callback handler to log sent or received messages (optional)
*/
void Server::rawMessageHandler(void *parameter, IMasterConnection con, uint8_t *msg, int msgSize, bool sent)
{
    if (sent)
        printf("SEND: ");
    else
        printf("RCVD: ");

    int i;
    for (i = 0; i < msgSize; i++)
    {
        printf("%02x ", msg[i]);
    }

    printf("\n");
}

/*
* Callback handler for interrogation messages
*/
bool Server::interrogationHandler(void *parameter, IMasterConnection connection, CS101_ASDU asdu, uint8_t qoi)
{
    printf("Received interrogation for group %i\n", qoi);

    if (qoi == 20) /* only handle station interrogation */
    {
        CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);
        IMasterConnection_sendACT_CON(connection, asdu, false);

        // Send binary values
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION, 0, 1, false, false);
        // kv = {<address>: {<tag>, eInput}}
        for (const auto &kv : gServer->mBinaryPoints)
        {
            const std::string tag = kv.second.first;
            if (CS101_ASDU_getPayloadSize(newAsdu) < MAX_ASDU_PAYLOAD_SIZE)
            {
                if (gServer->mDataManager->hasTag(tag))
                {
                    auto status = Server::convertBoolToDPValue(gServer->mDataManager->getDataByTag<bool>(tag));
                    InformationObject io = (InformationObject)DoublePointInformation_create(NULL, kv.first, status, IEC60870_QUALITY_GOOD);
                    CS101_ASDU_addInformationObject(newAsdu, io);
                    InformationObject_destroy(io);
                }
            }
            else
            {
                // Send current ASDU and create a new one for the remaining values
                IMasterConnection_sendASDU(connection, newAsdu);
                CS101_ASDU_destroy(newAsdu);
                newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION, 0, 1, false, false);
                if (gServer->mDataManager->hasTag(tag))
                {
                    auto status = Server::convertBoolToDPValue(gServer->mDataManager->getDataByTag<bool>(tag));
                    InformationObject io = (InformationObject)DoublePointInformation_create(NULL, kv.first, status, IEC60870_QUALITY_GOOD);
                    CS101_ASDU_addInformationObject(newAsdu, io);
                    InformationObject_destroy(io);
                }
            }
        }
        IMasterConnection_sendASDU(connection, newAsdu);
        CS101_ASDU_destroy(newAsdu);

        // Send analog values
        newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION, 0, 1, false, false);
        // kv = {<address>: {<tag>, eInput}}
        for (const auto &kv : gServer->mAnalogPoints)
        {
            const std::string tag = kv.second.first;
            if (CS101_ASDU_getPayloadSize(newAsdu) < MAX_ASDU_PAYLOAD_SIZE)
            {
                if (gServer->mDataManager->hasTag(tag))
                {
                    auto val = gServer->mDataManager->getDataByTag<double>(tag);
                    InformationObject io = (InformationObject)MeasuredValueShort_create(NULL, kv.first, val, IEC60870_QUALITY_GOOD);
                    CS101_ASDU_addInformationObject(newAsdu, io);
                    InformationObject_destroy(io);
                }
            }
            else
            {
                // Send current ASDU and create a new one for the remaining values
                IMasterConnection_sendASDU(connection, newAsdu);
                CS101_ASDU_destroy(newAsdu);
                newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION, 0, 1, false, false);
                if (gServer->mDataManager->hasTag(tag))
                {
                    auto val = gServer->mDataManager->getDataByTag<double>(tag);
                    InformationObject io = (InformationObject)MeasuredValueShort_create(NULL, kv.first, val, IEC60870_QUALITY_GOOD);
                    CS101_ASDU_addInformationObject(newAsdu, io);
                    InformationObject_destroy(io);
                }
            }
        }
        IMasterConnection_sendASDU(connection, newAsdu);
        CS101_ASDU_destroy(newAsdu);
        IMasterConnection_sendACT_TERM(connection, asdu);
    }
    else
    {
        IMasterConnection_sendACT_CON(connection, asdu, true);
    }

    return true;
}

bool Server::asduHandler(void *parameter, IMasterConnection connection, CS101_ASDU asdu)
{
    if (CS101_ASDU_getTypeID(asdu) == C_DC_NA_1)
    {
        printf("received double command\n");

        if (CS101_ASDU_getCOT(asdu) == CS101_COT_ACTIVATION)
        {
            InformationObject io = CS101_ASDU_getElement(asdu, 0);

            if (io)
            {
                // Send activation confirmation (application layer ACK)
                IMasterConnection_sendACT_CON(connection, asdu, false);

                DoubleCommand dc = (DoubleCommand)io;
                uint16_t addr = InformationObject_getObjectAddress(io);
                int state = DoubleCommand_getState(dc);
                printf("IOA: %i switch to %i\n", addr, state);
                gServer->writeBinary(addr, state);
                // Send activation termination
                CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_TERMINATION);
                InformationObject_destroy(io);
            }
            else
            {
                printf("ERROR: message has no valid information object\n");
                return true;
            }
        }
        else
            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);

        IMasterConnection_sendASDU(connection, asdu);

        return true;
    }
    else if (CS101_ASDU_getTypeID(asdu) == C_SE_NC_1)
    {
        printf("received setpoint command (float)\n");

        if (CS101_ASDU_getCOT(asdu) == CS101_COT_ACTIVATION)
        {
            InformationObject io = CS101_ASDU_getElement(asdu, 0);

            if (io)
            {
                SetpointCommandShort sc = (SetpointCommandShort)io;
                uint16_t addr = InformationObject_getObjectAddress(io);
                float value = SetpointCommandShort_getValue(sc);
                printf("IOA: %i switch to %f\n", addr, value);
                gServer->writeAnalog(addr, value);
                CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
                InformationObject_destroy(io);
            }
            else
            {
                printf("ERROR: message has no valid information object\n");
                return true;
            }
        }
        else
            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);

        IMasterConnection_sendASDU(connection, asdu);

        return true;
    }

    return false;
}

bool Server::connectionRequestHandler(void *parameter, const char *ipAddress)
{
    printf("New connection request from %s\n", ipAddress);
    return true;
}

void Server::connectionEventHandler(void *parameter, IMasterConnection con, CS104_PeerConnectionEvent event)
{
    if (event == CS104_CON_EVENT_CONNECTION_OPENED)
    {
        printf("Connection opened (%p)\n", con);
        gServer->mConnection = con;
        gServer->mConnected = true;
    }
    else if (event == CS104_CON_EVENT_CONNECTION_CLOSED)
    {
        printf("Connection closed (%p)\n", con);
        gServer->mConnected = false;
    }
    else if (event == CS104_CON_EVENT_ACTIVATED)
    {
        printf("Connection activated (%p)\n", con);
    }
    else if (event == CS104_CON_EVENT_DEACTIVATED)
    {
        printf("Connection deactivated (%p)\n", con);
    }
}

/*
* Write binary value to datastore.
*/
void Server::writeBinary(std::uint16_t address, int value)
{
    std::ostringstream log_stream;
    log_stream << "Binary point command at address " << address << " with value " << value << ".";
    logEvent("iec60870-5-104 Server writeBinary", "info", log_stream.str());
    auto iter = mBinaryPoints.find(address);
    if (iter == mBinaryPoints.end())
    {
        log_stream.str("");
        log_stream << "Invalid binary point command request address: " << address;
        logEvent("binary point command", "error", log_stream.str());
        return;
    }

    // Convert DoublePoint value to bool for datastore equivalence
    bool bvalue = 0;
    if (value == IEC60870_DOUBLE_POINT_OFF) 
    {
        bvalue = 0;
    }
    else if (value == IEC60870_DOUBLE_POINT_ON) 
    {
        bvalue = 1;
    }
    else 
    {
        log_stream << "Double Point value is in indeterminate state..defaulting to 0";
        logEvent("binary point command", "error", log_stream.str());
        bvalue = 0;
    }
    mDataManager->addUpdatedBinaryTag(iter->second.first, bvalue);
    log_stream.str("");
    log_stream << "Data successfully written.";
    logEvent("write binary", "info", log_stream.str());
}

/*
* Write analog value to datastore.
*/
void Server::writeAnalog(std::uint16_t address, float value)
{
    std::ostringstream log_stream;
    log_stream << "Analog point command at address " << address << " with value " << value << ".";
    logEvent("iec60870-5-104 Server writeAnalog", "info", log_stream.str());
    auto iter = mAnalogPoints.find(address);
    if (iter == mAnalogPoints.end())
    {
        log_stream.str("");
        log_stream << "Invalid analog point command request address: " << address;
        logEvent("analog point command", "error", log_stream.str());
        return;
    }
    mDataManager->addUpdatedAnalogTag(iter->second.first, value);
    log_stream.str("");
    log_stream << "Data successfully written.";
    logEvent("write analog", "info", log_stream.str());
}

bool Server::addBinaryInput(const std::uint16_t address, const std::string &tag)
{
    if (mDataManager->hasTag(tag))
    {
        mBinaryPoints[address] = std::make_pair(tag, PointType::eInput);
        return true;
    }
    return false;
}

bool Server::addBinaryOutput(const std::uint16_t address, const std::string &tag)
{
    if (mDataManager->hasTag(tag))
    {
        mBinaryPoints[address] = std::make_pair(tag, PointType::eOutput);
        return true;
    }
    return false;
}

bool Server::addAnalogInput(const std::uint16_t address, const std::string &tag)
{
    if (mDataManager->hasTag(tag))
    {
        mAnalogPoints[address] = std::make_pair(tag, PointType::eInput);
        return true;
    }
    return false;
}

bool Server::addAnalogOutput(const std::uint16_t address, const std::string &tag)
{
    if (mDataManager->hasTag(tag))
    {
        mAnalogPoints[address] = std::make_pair(tag, PointType::eOutput);
        return true;
    }
    return false;
}


} // namespace iec60870
} // namespace comms
} // namespace bennu
