#include "ClientConnection.hpp"

#include <iostream>
#include <functional>

namespace bennu {
namespace comms {
namespace iec60870 {

ClientConnection::ClientConnection(const std::string& rtuEndpoint) :
    mRunning(false),
    mRtuEndpoint(rtuEndpoint)
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

        CS104_Connection_setConnectionHandler(mConnection, connectionHandler, NULL);
        CS104_Connection_setASDUReceivedHandler(mConnection, asduReceivedHandler, NULL);

        /* uncomment to log messages */
        //CS104_Connection_setRawMessageHandler(mConnection, rawMessageHandler, NULL);

        if (CS104_Connection_connect(mConnection))
        {
            CS104_Connection_sendStartDT(mConnection);
            mRunning = true;
            // Send intial interrogation (no need for client to poll; server has reverse polling loop)
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

StatusMessage ClientConnection::writeBinary(const std::string& tag, bool value)
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

    // Write boolean using protocol
    std::cout << "Send single command C_SC_NA_1: " << tag << " -- " << value << std::endl;
    InformationObject sc = (InformationObject)
            SingleCommand_create(NULL, rd.mRegisterAddress, value, true, 0);
    CS104_Connection_sendProcessCommandEx(mConnection, CS101_COT_ACTIVATION, 1, sc);
    InformationObject_destroy(sc);

    // Update local data so we don't have to wait until the next poll
    updateBinary(rd.mRegisterAddress, value);
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
    // Write analog using protocol
    std::cout << "Send setpoint command C_SE_NC_1: " << tag << " -- " << value << std::endl;
    InformationObject sc = (InformationObject)
            SetpointCommandShort_create(NULL, rd.mRegisterAddress, value, true, 0);
    CS104_Connection_sendProcessCommandEx(mConnection, CS101_COT_ACTIVATION, 1, sc);

    // Update local data so we don't have to wait until the next poll
    updateAnalog(rd.mRegisterAddress, value);
    return sm;
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
    switch (event) {
    case CS104_CONNECTION_OPENED:
        printf("Connection established\n");
        break;
    case CS104_CONNECTION_CLOSED:
        printf("Connection closed\n");
        break;
    case CS104_CONNECTION_STARTDT_CON_RECEIVED:
        printf("Received STARTDT_CON\n");
        break;
    case CS104_CONNECTION_STOPDT_CON_RECEIVED:
        printf("Received STOPDT_CON\n");
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
    printf("RECVD ASDU type: %s(%i) elements: %i\n",
            TypeID_toString(CS101_ASDU_getTypeID(asdu)),
            CS101_ASDU_getTypeID(asdu),
            CS101_ASDU_getNumberOfElements(asdu));

    // Analog values
    if (CS101_ASDU_getTypeID(asdu) == M_ME_NC_1) {

        printf("  measured short values:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            MeasuredValueShort io =
                    (MeasuredValueShort) CS101_ASDU_getElement(asdu, i);

            uint16_t addr = InformationObject_getObjectAddress((InformationObject) io);
            double value = MeasuredValueShort_getValue((MeasuredValueShort) io);
            printf("    IOA: %i value: %f\n", addr, value);

            gClientConnection->updateAnalog(addr, value);

            MeasuredValueShort_destroy(io);
        }
    }
    // Binary values
    else if (CS101_ASDU_getTypeID(asdu) == M_SP_NA_1) {
        printf("  single point information:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            SinglePointInformation io =
                    (SinglePointInformation) CS101_ASDU_getElement(asdu, i);

            uint16_t addr = InformationObject_getObjectAddress((InformationObject) io);
            bool status = SinglePointInformation_getValue((SinglePointInformation) io);
            printf("    IOA: %i value: %i\n", addr, status);

            gClientConnection->updateBinary(addr, status);

            SinglePointInformation_destroy(io);
        }
    }

    return true;
}

} // namespace iec60870
} // namespace comms
} // namespace bennu
