#include "ClientConnection.hpp"

#include <iostream>
#include <functional>

namespace bennu {
namespace comms {
namespace iec60870 {

ClientConnection::ClientConnection(const std::string& rtuEndpoint, bool sboEnabled) :
    mRunning(false),
    mSboEnabled(sboEnabled),
    mRtuEndpoint(rtuEndpoint),
    mConnection(nullptr)
{
}

void ClientConnection::start(std::shared_ptr<ClientConnection> clientConnection)
{
    // Set static shared_ptr to client connection instance so it can be used inside static 104 callback handlers
    gClientConnection = clientConnection;
    // If endpoint starts with tcp://, parse ip/port and use TCP
    std::size_t findResult = mRtuEndpoint.find("tcp://");

    if (findResult != std::string::npos)
    {
        // Handle splitting out ip and port from endpoint
        std::string ipAndPort = mRtuEndpoint.substr(findResult + 6);
        std::string ip = ipAndPort.substr(0, ipAndPort.find(":"));
        std::uint16_t port = static_cast<std::uint16_t>(stoi(ipAndPort.substr(ipAndPort.find(":") + 1)));

        printf("Connecting to: %s:%i\n", ip.data(), port);
        mConnection = CS104_Connection_create(ip.data(), port);

        CS101_AppLayerParameters alParams = CS104_Connection_getAppLayerParameters(mConnection);
        alParams->originatorAddress = 3;

        CS104_Connection_setConnectionHandler(mConnection, connectionHandler, this);
        CS104_Connection_setASDUReceivedHandler(mConnection, asduReceivedHandler, this);

        /* uncomment to log messages */
        //CS104_Connection_setRawMessageHandler(mConnection, rawMessageHandler, NULL);

        if (CS104_Connection_connect(mConnection))
        {
            CS104_Connection_sendStartDT(mConnection);
            mRunning = true;
            // Send initial interrogation (no need for client to poll; server has reverse polling loop)
            CS104_Connection_sendInterrogationCommand(mConnection, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);
            std::cout << "Started IEC60870-5-104-CLIENT -- RTU Connection: " << mRtuEndpoint << std::endl;
        }
        else
        {
            std::cout << "Error -- Could not start IEC60870-5-104-CLIENT -- RTU Connection: " << mRtuEndpoint << std::endl;
            std::cout << "Exiting." << std::endl;
        }
    }
    else
    {
        std::cout << "Error -- Unknown endpoint protocol: " << mRtuEndpoint << std::endl;
        exit(-1);
    }
}

int ClientConnection::convertBoolToDPValue(bool value)
{
    if (value == 0)
        return IEC60870_DOUBLE_POINT_OFF;
    else
        return IEC60870_DOUBLE_POINT_ON;
}

StatusMessage ClientConnection::readRegisterByTag(const std::string& tag, comms::RegisterDescriptor& rd)
{
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    StatusMessage sm = STATUS_INIT;
    sm.status = status;
    if (!status)
    {
        auto msg = "readRegisterByTag(): Unable to find tag -- " + tag;
        sm.message = msg.data();
    }
    return sm;
}

StatusMessage ClientConnection::writeBinary(const std::string& tag, bool bvalue)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        auto msg = "writeBinary(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = msg.data();
        return sm;
    }

    // Convert boolean to double point value
    int value = ClientConnection::convertBoolToDPValue(bvalue);

    if (mSboEnabled)
    {
        // Select-before-operate: send SELECT now, defer EXECUTE until the
        // positive ACT_CON arrives (handled in asduReceivedHandler).
        {
            std::lock_guard<std::mutex> lock(mPendingMutex);
            if (mPendingCommands.find(rd.mRegisterAddress) != mPendingCommands.end())
            {
                sm.status = STATUS_FAIL;
                sm.message = (char*)"writeBinary(): SELECT already pending for IOA";
                return sm;
            }
            mPendingCommands[rd.mRegisterAddress] = PendingCommand{ePendingDouble, value, 0.0f};
        }
        std::cout << "Send SELECT double command C_DC_NA_1: " << tag << " -- " << bvalue << std::endl;
        InformationObject dc = (InformationObject)
            DoubleCommand_create(NULL, rd.mRegisterAddress, value, true, 0);
        bool sent = CS104_Connection_sendProcessCommandEx(mConnection, CS101_COT_ACTIVATION, 1, dc);
        InformationObject_destroy(dc);
        if (!sent)
        {
            std::lock_guard<std::mutex> lock(mPendingMutex);
            mPendingCommands.erase(rd.mRegisterAddress);
            sm.status = STATUS_FAIL;
            sm.message = (char*)"writeBinary(): Failed to send SELECT command";
        }
        // Note: local data is updated only once the EXECUTE is sent.
        return sm;
    }

    // Direct operate (SBO disabled): send EXECUTE (selectCommand=false).
    std::cout << "Send double command C_DC_NA_1: " << tag << " -- " << bvalue << std::endl;
    InformationObject dc = (InformationObject)
        DoubleCommand_create(NULL, rd.mRegisterAddress, value, false, 0);
    CS104_Connection_sendProcessCommandEx(mConnection, CS101_COT_ACTIVATION, 1, dc);
    InformationObject_destroy(dc);

    // Update local data so we don't have to wait until the next poll; save boolean to datastore
    updateBinary(rd.mRegisterAddress, bvalue);
    return sm;
}

StatusMessage ClientConnection::writeAnalog(const std::string& tag, double value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        auto msg = "writeAnalog(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = msg.data();
        return sm;
    }
    if (mSboEnabled)
    {
        // Select-before-operate: send SELECT now, defer EXECUTE until the
        // positive ACT_CON arrives (handled in asduReceivedHandler).
        {
            std::lock_guard<std::mutex> lock(mPendingMutex);
            if (mPendingCommands.find(rd.mRegisterAddress) != mPendingCommands.end())
            {
                sm.status = STATUS_FAIL;
                sm.message = (char*)"writeAnalog(): SELECT already pending for IOA";
                return sm;
            }
            mPendingCommands[rd.mRegisterAddress] = PendingCommand{ePendingSetpoint, 0, static_cast<float>(value)};
        }
        std::cout << "Send SELECT setpoint command C_SE_NC_1: " << tag << " -- " << value << std::endl;
        InformationObject sc = (InformationObject)
            SetpointCommandShort_create(NULL, rd.mRegisterAddress, static_cast<float>(value), true, 0);
        bool sent = CS104_Connection_sendProcessCommandEx(mConnection, CS101_COT_ACTIVATION, 1, sc);
        InformationObject_destroy(sc);
        if (!sent)
        {
            std::lock_guard<std::mutex> lock(mPendingMutex);
            mPendingCommands.erase(rd.mRegisterAddress);
            sm.status = STATUS_FAIL;
            sm.message = (char*)"writeAnalog(): Failed to send SELECT command";
        }
        // Note: local data is updated only once the EXECUTE is sent.
        return sm;
    }

    // Direct operate (SBO disabled): send EXECUTE (selectCommand=false).
    std::cout << "Send setpoint command C_SE_NC_1: " << tag << " -- " << value << std::endl;
    InformationObject sc = (InformationObject)
        SetpointCommandShort_create(NULL, rd.mRegisterAddress, static_cast<float>(value), false, 0);
    CS104_Connection_sendProcessCommandEx(mConnection, CS101_COT_ACTIVATION, 1, sc);
    InformationObject_destroy(sc);

    // Update local data so we don't have to wait until the next poll
    updateAnalog(rd.mRegisterAddress, value);
    return sm;
}

/*
 * Send the EXECUTE phase for a pending select-before-operate command at the given
 * IOA. Called from asduReceivedHandler when a SELECT's positive ACT_CON arrives.
 */
void ClientConnection::sendExecute(std::uint16_t address, PendingCommandType expectedType)
{
    PendingCommand pending;
    {
        std::lock_guard<std::mutex> lock(mPendingMutex);
        auto iter = mPendingCommands.find(address);
        if (iter == mPendingCommands.end() || iter->second.type != expectedType)
        {
            return;
        }
        pending = iter->second;
        mPendingCommands.erase(iter);
    }

    if (pending.type == ePendingDouble)
    {
        std::cout << "Send EXECUTE double command C_DC_NA_1: IOA " << address << " -- " << pending.value << std::endl;
        InformationObject dc = (InformationObject)
            DoubleCommand_create(NULL, address, pending.value, false, 0);
        CS104_Connection_sendProcessCommandEx(mConnection, CS101_COT_ACTIVATION, 1, dc);
        InformationObject_destroy(dc);
        // Update local data now that the operate has been issued.
        updateBinary(address, pending.value == IEC60870_DOUBLE_POINT_ON);
    }
    else // ePendingSetpoint
    {
        std::cout << "Send EXECUTE setpoint command C_SE_NC_1: IOA " << address << " -- " << pending.fValue << std::endl;
        InformationObject sc = (InformationObject)
            SetpointCommandShort_create(NULL, address, pending.fValue, false, 0);
        CS104_Connection_sendProcessCommandEx(mConnection, CS101_COT_ACTIVATION, 1, sc);
        InformationObject_destroy(sc);
        // Update local data now that the operate has been issued.
        updateAnalog(address, pending.fValue);
    }
}

/*
 * Callback handler to log sent or received messages (optional)
 */
void ClientConnection::rawMessageHandler(void* parameter, uint8_t* msg, int msgSize, bool sent)
{
    if (sent)
        printf("SEND: ");
    else
        printf("RCVD: ");

    int i;
    for (i = 0; i < msgSize; i++) {
        printf("%02x ", msg[i]);
    }

    printf("\n");
}

/*
 * Callback handler for connections
 */
void ClientConnection::connectionHandler(void* parameter, CS104_Connection connection, CS104_ConnectionEvent event)
{
    ClientConnection* self = parameter ? static_cast<ClientConnection*>(parameter) : gClientConnection.get();
    switch (event) {
    case CS104_CONNECTION_OPENED:
        printf("Connection established\n");
        break;
    case CS104_CONNECTION_CLOSED:
        printf("Connection closed\n");
        if (self)
        {
            std::lock_guard<std::mutex> lock(self->mPendingMutex);
            self->mPendingCommands.clear();
        }
        break;
    case CS104_CONNECTION_STARTDT_CON_RECEIVED:
        printf("Received STARTDT_CON\n");
        break;
    case CS104_CONNECTION_STOPDT_CON_RECEIVED:
        printf("Received STOPDT_CON\n");
        if (self)
        {
            std::lock_guard<std::mutex> lock(self->mPendingMutex);
            self->mPendingCommands.clear();
        }
        break;
    }
}

/*
 * CS101_ASDUReceivedHandler implementation
 *
 * For CS104 the address parameter has to be ignored
 */
bool ClientConnection::asduReceivedHandler(void* parameter, int address, CS101_ASDU asdu)
{
    ClientConnection* self = parameter ? static_cast<ClientConnection*>(parameter) : gClientConnection.get();
    if (!self)
    {
        return false;
    }

    printf("RECVD ASDU type: %s(%i) elements: %i\n",
            TypeID_toString(CS101_ASDU_getTypeID(asdu)),
            CS101_ASDU_getTypeID(asdu),
            CS101_ASDU_getNumberOfElements(asdu));

    IEC60870_5_TypeID typeId = CS101_ASDU_getTypeID(asdu);

    // Command responses (ACT_CON / ACT_TERM) for the control command types we send.
    if (typeId == C_DC_NA_1 || typeId == C_SE_NC_1)
    {
        CS101_CauseOfTransmission cot = CS101_ASDU_getCOT(asdu);
        if (cot == CS101_COT_ACTIVATION_CON)
        {
            InformationObject io = CS101_ASDU_getElement(asdu, 0);
            if (io)
            {
                uint16_t addr = InformationObject_getObjectAddress(io);
                bool isSelect = (typeId == C_DC_NA_1)
                                    ? DoubleCommand_isSelect((DoubleCommand)io)
                                    : SetpointCommandShort_isSelect((SetpointCommandShort)io);
                PendingCommandType expectedType = (typeId == C_DC_NA_1) ? ePendingDouble : ePendingSetpoint;

                if (isSelect)
                {
                    if (CS101_ASDU_isNegative(asdu))
                    {
                        // SELECT was rejected; drop any matching pending operate.
                        printf("  SELECT command IOA %i rejected (negative ACT_CON)\n", addr);
                        {
                            std::lock_guard<std::mutex> lock(self->mPendingMutex);
                            auto iter = self->mPendingCommands.find(addr);
                            if (iter != self->mPendingCommands.end() && iter->second.type == expectedType)
                            {
                                self->mPendingCommands.erase(iter);
                            }
                        }
                    }
                    else
                    {
                        // Positive confirmation for SELECT -- follow up with the EXECUTE.
                        printf("  SELECT command IOA %i confirmed (positive ACT_CON)\n", addr);
                        self->sendExecute(addr, expectedType);
                    }
                }
                else
                {
                    if (CS101_ASDU_isNegative(asdu))
                    {
                        printf("  EXECUTE command IOA %i rejected (negative ACT_CON)\n", addr);
                    }
                    else
                    {
                        printf("  EXECUTE command IOA %i confirmed (positive ACT_CON)\n", addr);
                    }
                }
                InformationObject_destroy(io);
            }
        }
        return true;
    }

    // Analog values
    if (typeId == M_ME_NC_1)
    {

        printf("  measured short values:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            MeasuredValueShort io =
                    (MeasuredValueShort) CS101_ASDU_getElement(asdu, i);

            uint16_t addr = InformationObject_getObjectAddress((InformationObject) io);
            double value = MeasuredValueShort_getValue((MeasuredValueShort) io);
            printf("    IOA: %i value: %f\n", addr, value);

            self->updateAnalog(addr, value);

            MeasuredValueShort_destroy(io);
        }
    }
    // Binary values
    else if (CS101_ASDU_getTypeID(asdu) == M_DP_NA_1) {
        printf("  double point information:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            DoublePointInformation io =
                    (DoublePointInformation) CS101_ASDU_getElement(asdu, i);

            uint16_t addr = InformationObject_getObjectAddress((InformationObject) io);
            DoublePointValue dp_value = DoublePointInformation_getValue((DoublePointInformation) io);

            bool status = 0;
            if (dp_value == IEC60870_DOUBLE_POINT_OFF)
            {
                status = 0;
            }
            else if (dp_value == IEC60870_DOUBLE_POINT_ON)
            {
                status = 1;
            }
            else
            {
                printf("IOA: %i- Double point value is in indeterminate state..defaulting to 0\n", addr);
                status = 0;
            }
            printf("    IOA: %i value: %i\n", addr, status);

            self->updateBinary(addr, status);

            DoublePointInformation_destroy(io);
        }
    }

    return true;
}


} // namespace iec60870
} // namespace comms
} // namespace bennu
