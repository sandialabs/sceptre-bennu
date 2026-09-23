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
    mConnected(false),
    mReversePollRate(0),
    mConnection(nullptr),
    mSboEnabled(false),
    mSboTimeoutMs(DEFAULT_SBO_TIMEOUT_SECS * 1000)
{
    setDataManager(dm);
}

void Server::start(const std::string& endpoint, std::shared_ptr<Server> server, const uint32_t rPollRate, std::string subtype, bool sboEnabled, uint32_t sboTimeoutSecs)
{
    // Set server reverse-poll rate
    mReversePollRate = rPollRate;
    // Set select-before-operate configuration
    mSboEnabled = sboEnabled;
    mSboTimeoutMs = static_cast<int64_t>(sboTimeoutSecs) * 1000;
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
        if (subtype.find("double") != std::string::npos)
        {
            CS104_Slave_setInterrogationHandler(slave, interrogationHandlerDoublePoint, NULL);
        }
        else
        {
            CS104_Slave_setInterrogationHandler(slave, interrogationHandlerSinglePoint, NULL);
        }
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
            if (subtype.find("double") != std::string::npos)
            {
                pServerPollThread.reset(new std::thread(std::bind(&Server::reversePollDoublePoint, this)));
            }
            else
            {
                pServerPollThread.reset(new std::thread(std::bind(&Server::reversePollSinglePoint, this)));
            }
        }

    }
    else
    {
        logEvent("iec60870-5-104 server start", "error", "Unknown endpoint protocol (" + endpoint + ")");
        return;
    }

    // Log data size
    std::ostringstream log_stream;
    log_stream << "Initialized IEC60870-5-104 server: " << endpoint
               << " (select-before-operate " << (mSboEnabled ? "enabled" : "disabled") << ")";
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
        case 1:
            return IEC60870_DOUBLE_POINT_OFF;
        case 2:
            return IEC60870_DOUBLE_POINT_ON;
        case 3:
            return IEC60870_DOUBLE_POINT_INDETERMINATE;
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
 * connected clients. Sends indications as single point values
 */
void Server::reversePollSinglePoint()
{
    while (1)
    {
        if (mConnected)
        {
            CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(mConnection);

            // Send binary values
            CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);
            // kv = {<address>: {<tag>, eInput}}
            for (const auto &kv : gServer->mBinaryPoints)
            {
                const std::string tag = kv.second.first;
                if (CS101_ASDU_getPayloadSize(newAsdu) >= MAX_ASDU_PAYLOAD_SIZE)
                {
                    // Send current ASDU and create a new one for the remaining values
                    IMasterConnection_sendASDU(mConnection, newAsdu);
                    CS101_ASDU_destroy(newAsdu);
                    newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);
                }

                if (gServer->mDataManager->hasTag(tag))
                {
                    auto status = gServer->mDataManager->getDataByTag<bool>(tag);
                    InformationObject io = (InformationObject)SinglePointInformation_create(NULL, kv.first, status, IEC60870_QUALITY_GOOD);
                    CS101_ASDU_addInformationObject(newAsdu, io);
                    InformationObject_destroy(io);
                }
            }
            IMasterConnection_sendASDU(mConnection, newAsdu);
            CS101_ASDU_destroy(newAsdu);

            // Send analog values
            newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);
            // kv = {<address>: {<tag>, eInput}}
            for (const auto &kv : gServer->mAnalogPoints)
            {
                const std::string tag = kv.second.first;
                if (CS101_ASDU_getPayloadSize(newAsdu) >= MAX_ASDU_PAYLOAD_SIZE)
                {
                    // Send current ASDU and create a new one for the remaining values
                    IMasterConnection_sendASDU(mConnection, newAsdu);
                    CS101_ASDU_destroy(newAsdu);
                    newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);
                }

                if (gServer->mDataManager->hasTag(tag))
                {
                    auto val = gServer->mDataManager->getDataByTag<double>(tag);
                    InformationObject io = (InformationObject)MeasuredValueShort_create(NULL, kv.first, val, IEC60870_QUALITY_GOOD);
                    CS101_ASDU_addInformationObject(newAsdu, io);
                    InformationObject_destroy(io);
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
 * Reverse poll loop that sends local bennu datastore data to
 * connected clients. Sends indications as double point values
 */
void Server::reversePollDoublePoint()
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
                if (CS101_ASDU_getPayloadSize(newAsdu) >= MAX_ASDU_PAYLOAD_SIZE)
                {
                    // Send current ASDU and create a new one for the remaining values
                    IMasterConnection_sendASDU(mConnection, newAsdu);
                    CS101_ASDU_destroy(newAsdu);
                    newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);
                }

                if (gServer->mDataManager->hasTag(tag))
                {
                    auto val = gServer->mDataManager->getDataByTag<double>(tag);
                    InformationObject io = (InformationObject)MeasuredValueShort_create(NULL, kv.first, val, IEC60870_QUALITY_GOOD);
                    CS101_ASDU_addInformationObject(newAsdu, io);
                    InformationObject_destroy(io);
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
* Callback handler for interrogation messages that reports indications as single point values
*/
bool Server::interrogationHandlerSinglePoint(void *parameter, IMasterConnection connection, CS101_ASDU asdu, uint8_t qoi)
{
    std::cout << "Received interrogation for group " << static_cast<int16_t>(qoi) << std::endl;

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
            if (CS101_ASDU_getPayloadSize(newAsdu) >= MAX_ASDU_PAYLOAD_SIZE)
            {
                // Send current ASDU and create a new one for the remaining values
                IMasterConnection_sendASDU(connection, newAsdu);
                CS101_ASDU_destroy(newAsdu);
                newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION, 0, 1, false, false);
            }

            if (gServer->mDataManager->hasTag(tag))
            {
                auto status = gServer->mDataManager->getDataByTag<bool>(tag);
                InformationObject io = (InformationObject)SinglePointInformation_create(NULL, kv.first, status, IEC60870_QUALITY_GOOD);
                CS101_ASDU_addInformationObject(newAsdu, io);
                InformationObject_destroy(io);
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
            if (CS101_ASDU_getPayloadSize(newAsdu) >= MAX_ASDU_PAYLOAD_SIZE)
            {
                // Send current ASDU and create a new one for the remaining values
                IMasterConnection_sendASDU(connection, newAsdu);
                CS101_ASDU_destroy(newAsdu);
                newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION, 0, 1, false, false);
            }

            if (gServer->mDataManager->hasTag(tag))
            {
                auto val = gServer->mDataManager->getDataByTag<double>(tag);
                InformationObject io = (InformationObject)MeasuredValueShort_create(NULL, kv.first, val, IEC60870_QUALITY_GOOD);
                CS101_ASDU_addInformationObject(newAsdu, io);
                InformationObject_destroy(io);
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

/*
* Callback handler for interrogation messages that reports indications as double point values
*/
bool Server::interrogationHandlerDoublePoint(void *parameter, IMasterConnection connection, CS101_ASDU asdu, uint8_t qoi)
{
    std::cout << "Received interrogation for group " << static_cast<int16_t>(qoi) << std::endl;

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
            if (CS101_ASDU_getPayloadSize(newAsdu) >= MAX_ASDU_PAYLOAD_SIZE)
            {
                // Send current ASDU and create a new one for the remaining values
                IMasterConnection_sendASDU(connection, newAsdu);
                CS101_ASDU_destroy(newAsdu);
                newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION, 0, 1, false, false);
            }

            if (gServer->mDataManager->hasTag(tag))
            {
                auto status = Server::convertBoolToDPValue(gServer->mDataManager->getDataByTag<bool>(tag));
                InformationObject io = (InformationObject)DoublePointInformation_create(NULL, kv.first, status, IEC60870_QUALITY_GOOD);
                CS101_ASDU_addInformationObject(newAsdu, io);
                InformationObject_destroy(io);
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
            if (CS101_ASDU_getPayloadSize(newAsdu) >= MAX_ASDU_PAYLOAD_SIZE)
            {
                // Send current ASDU and create a new one for the remaining values
                IMasterConnection_sendASDU(connection, newAsdu);
                CS101_ASDU_destroy(newAsdu);
                newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION, 0, 1, false, false);
            }

            if (gServer->mDataManager->hasTag(tag))
            {
                auto val = gServer->mDataManager->getDataByTag<double>(tag);
                InformationObject io = (InformationObject)MeasuredValueShort_create(NULL, kv.first, val, IEC60870_QUALITY_GOOD);
                CS101_ASDU_addInformationObject(newAsdu, io);
                InformationObject_destroy(io);
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
    IEC60870_5_TypeID typeId = CS101_ASDU_getTypeID(asdu);

    if (typeId == C_SC_NA_1)
    {
        std::cout << "received single command" << std::endl;

        CS101_CauseOfTransmission cot = CS101_ASDU_getCOT(asdu);
        if (cot == CS101_COT_DEACTIVATION)
        {
            // Cancel any outstanding selection for this point.
            InformationObject cancelIo = CS101_ASDU_getElement(asdu, 0);
            if (cancelIo)
            {
                gServer->clearSelect(InformationObject_getObjectAddress(cancelIo));
                InformationObject_destroy(cancelIo);
            }
            CS101_ASDU_setCOT(asdu, CS101_COT_DEACTIVATION_CON);
            IMasterConnection_sendASDU(connection, asdu);
            return true;
        }
        if (cot != CS101_COT_ACTIVATION)
        {
            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
            IMasterConnection_sendASDU(connection, asdu);
            return true;
        }

        InformationObject io = CS101_ASDU_getElement(asdu, 0);
        if (!io)
        {
            std::cout << "ERROR: message has no valid information object" << std::endl;
            return false;
        }

        SingleCommand sc = (SingleCommand)io;
        uint16_t addr = InformationObject_getObjectAddress(io);
        bool state = SingleCommand_getState(sc);
        bool isSelect = SingleCommand_isSelect(sc);

        if (gServer->isSboEnabled() && isSelect)
        {
            // SELECT: reserve the point; do NOT change the process value.
            bool ok = gServer->select(addr, connection);
            printf("SELECT single command IOA: %i (%s)\n", addr, ok ? "accepted" : "rejected");
            IMasterConnection_sendACT_CON(connection, asdu, !ok);
        }
        else
        {
            // EXECUTE (or direct-operate when SBO disabled).
            if (gServer->isSboEnabled() && !gServer->checkAndConsumeSelect(addr, connection))
            {
                printf("EXECUTE single command IOA: %i rejected (no valid select)\n", addr);
                IMasterConnection_sendACT_CON(connection, asdu, true);
            }
            else
            {
                printf("IOA: %i switch to %i\n", addr, state);
                gServer->writeBinary(addr, state);
                IMasterConnection_sendACT_CON(connection, asdu, false);
            }
        }

        InformationObject_destroy(io);
        return true;
    }
    else if (typeId == C_DC_NA_1)
    {
        std::cout << "received double command" << std::endl;

        CS101_CauseOfTransmission cot = CS101_ASDU_getCOT(asdu);
        if (cot == CS101_COT_DEACTIVATION)
        {
            // Cancel any outstanding selection for this point.
            InformationObject cancelIo = CS101_ASDU_getElement(asdu, 0);
            if (cancelIo)
            {
                gServer->clearSelect(InformationObject_getObjectAddress(cancelIo));
                InformationObject_destroy(cancelIo);
            }
            CS101_ASDU_setCOT(asdu, CS101_COT_DEACTIVATION_CON);
            IMasterConnection_sendASDU(connection, asdu);
            return true;
        }
        if (cot != CS101_COT_ACTIVATION)
        {
            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
            IMasterConnection_sendASDU(connection, asdu);
            return true;
        }

        InformationObject io = CS101_ASDU_getElement(asdu, 0);
        if (!io)
        {
            std::cout << "ERROR: message has no valid information object" << std::endl;
            return false;
        }

        DoubleCommand dc = (DoubleCommand)io;
        uint16_t addr = InformationObject_getObjectAddress(io);
        int state = DoubleCommand_getState(dc);
        bool isSelect = DoubleCommand_isSelect(dc);

        if (gServer->isSboEnabled() && isSelect)
        {
            // SELECT: reserve the point; do NOT change the process value.
            bool ok = gServer->select(addr, connection);
            printf("SELECT double command IOA: %i (%s)\n", addr, ok ? "accepted" : "rejected");
            IMasterConnection_sendACT_CON(connection, asdu, !ok);
        }
        else
        {
            // EXECUTE (or direct-operate when SBO disabled).
            if (gServer->isSboEnabled() && !gServer->checkAndConsumeSelect(addr, connection))
            {
                printf("EXECUTE double command IOA: %i rejected (no valid select)\n", addr);
                IMasterConnection_sendACT_CON(connection, asdu, true);
            }
            else
            {
                // Send activation confirmation (application layer ACK)
                IMasterConnection_sendACT_CON(connection, asdu, false);
                printf("IOA: %i switch to %i\n", addr, state);
                gServer->writeBinary(addr, state);
                // Send activation termination
                IMasterConnection_sendACT_TERM(connection, asdu);
            }
        }

        InformationObject_destroy(io);
        return true;
    }
    else if (typeId == C_SE_NC_1)
    {
        printf("received setpoint command (float)\n");

        CS101_CauseOfTransmission cot = CS101_ASDU_getCOT(asdu);
        if (cot == CS101_COT_DEACTIVATION)
        {
            // Cancel any outstanding selection for this point.
            InformationObject cancelIo = CS101_ASDU_getElement(asdu, 0);
            if (cancelIo)
            {
                gServer->clearSelect(InformationObject_getObjectAddress(cancelIo));
                InformationObject_destroy(cancelIo);
            }
            CS101_ASDU_setCOT(asdu, CS101_COT_DEACTIVATION_CON);
            IMasterConnection_sendASDU(connection, asdu);
            return true;
        }
        if (cot != CS101_COT_ACTIVATION)
        {
            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
            IMasterConnection_sendASDU(connection, asdu);
            return true;
        }

        InformationObject io = CS101_ASDU_getElement(asdu, 0);
        if (!io)
        {
            printf("ERROR: message has no valid information object\n");
            return false;
        }

        SetpointCommandShort sc = (SetpointCommandShort)io;
        uint16_t addr = InformationObject_getObjectAddress(io);
        float value = SetpointCommandShort_getValue(sc);
        bool isSelect = SetpointCommandShort_isSelect(sc);

        if (gServer->isSboEnabled() && isSelect)
        {
            // SELECT: reserve the point; do NOT change the process value.
            bool ok = gServer->select(addr, connection);
            printf("SELECT setpoint command IOA: %i (%s)\n", addr, ok ? "accepted" : "rejected");
            IMasterConnection_sendACT_CON(connection, asdu, !ok);
        }
        else
        {
            // EXECUTE (or direct-operate when SBO disabled).
            if (gServer->isSboEnabled() && !gServer->checkAndConsumeSelect(addr, connection))
            {
                printf("EXECUTE setpoint command IOA: %i rejected (no valid select)\n", addr);
                IMasterConnection_sendACT_CON(connection, asdu, true);
            }
            else
            {
                printf("IOA: %i switch to %f\n", addr, value);
                gServer->writeAnalog(addr, value);
                IMasterConnection_sendACT_CON(connection, asdu, false);
            }
        }

        InformationObject_destroy(io);
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
* Write binary value to datastore. Note this variant takes a bool argument (for single point indications)
*/
void Server::writeBinary(std::uint16_t address, bool value)
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
    mDataManager->addUpdatedBinaryTag(iter->second.first, value);
    log_stream.str("");
    log_stream << "Data successfully written.";
    logEvent("write binary", "info", log_stream.str());
}

/*
* Write binary value to datastore. Note this variant takes an int argument (for double point indications)
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

bool Server::isKnownPoint(uint16_t ioa) const
{
    return (mBinaryPoints.find(ioa) != mBinaryPoints.end()) ||
           (mAnalogPoints.find(ioa) != mAnalogPoints.end());
}

bool Server::select(uint16_t ioa, IMasterConnection connection)
{
    if (!isKnownPoint(ioa))
    {
        std::ostringstream log_stream;
        log_stream << "Rejected select for unknown IOA: " << ioa;
        logEvent("iec60870-5-104 select", "error", log_stream.str());
        return false;
    }

    std::lock_guard<std::mutex> lock(mSelectionMutex);
    mSelections[ioa] = SelectionState{connection, static_cast<int64_t>(Hal_getTimeInMs())};
    std::ostringstream log_stream;
    log_stream << "Point at IOA " << ioa << " selected.";
    logEvent("iec60870-5-104 select", "info", log_stream.str());
    return true;
}

bool Server::checkAndConsumeSelect(uint16_t ioa, IMasterConnection connection)
{
    std::lock_guard<std::mutex> lock(mSelectionMutex);
    auto iter = mSelections.find(ioa);
    if (iter == mSelections.end())
    {
        return false;
    }

    const SelectionState& sel = iter->second;
    bool sameConnection = (sel.connection == connection);
    bool expired = (static_cast<int64_t>(Hal_getTimeInMs()) - sel.selectedAtMs) > mSboTimeoutMs;

    // Consume the selection regardless of validity; a failed execute must
    // require a fresh select.
    mSelections.erase(iter);

    if (!sameConnection)
    {
        std::ostringstream log_stream;
        log_stream << "Execute for IOA " << ioa << " rejected: selected by a different connection.";
        logEvent("iec60870-5-104 execute", "error", log_stream.str());
        return false;
    }
    if (expired)
    {
        std::ostringstream log_stream;
        log_stream << "Execute for IOA " << ioa << " rejected: selection expired.";
        logEvent("iec60870-5-104 execute", "error", log_stream.str());
        return false;
    }
    return true;
}

void Server::clearSelect(uint16_t ioa)
{
    std::lock_guard<std::mutex> lock(mSelectionMutex);
    mSelections.erase(ioa);
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
