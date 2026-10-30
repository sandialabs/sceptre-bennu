#include "doctest.h"

#include <memory>
#include <string>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/comms/iec60870-5/module/ClientConnection.hpp"
#include "bennu/devices/modules/comms/iec60870-5/module/Server.hpp"

extern std::string exec(const char*);

using bennu::comms::eStatusReadWrite;
using bennu::comms::eValueReadWrite;
using bennu::comms::RegisterDescriptor;
using bennu::comms::iec60870::ClientConnection;
using bennu::comms::iec60870::gClientConnection;
using bennu::comms::iec60870::Server;
using bennu::field_device::DataManager;

namespace {

bool holdSelectAsduHandler(void* /*parameter*/, IMasterConnection /*connection*/, CS101_ASDU /*asdu*/)
{
    // Intentionally do not auto-send ACT_CON so the test can deterministically
    // verify pending-select behavior before confirmation arrives.
    return true;
}

struct MockMasterConnectionState
{
    int actConCalls = 0;
    bool lastActConNegative = false;
    int actTermCalls = 0;
    int sendAsduCalls = 0;
    CS101_CauseOfTransmission lastSentCot = CS101_COT_UNKNOWN_COT;
    bool lastSentNegative = false;
};

bool mockSendASDU(IMasterConnection self, CS101_ASDU asdu)
{
    auto* state = static_cast<MockMasterConnectionState*>(self->object);
    if (state)
    {
        ++state->sendAsduCalls;
        state->lastSentCot = CS101_ASDU_getCOT(asdu);
        state->lastSentNegative = CS101_ASDU_isNegative(asdu);
    }
    return true;
}

bool mockSendACT_CON(IMasterConnection self, CS101_ASDU /*asdu*/, bool negative)
{
    auto* state = static_cast<MockMasterConnectionState*>(self->object);
    if (state)
    {
        ++state->actConCalls;
        state->lastActConNegative = negative;
    }
    return true;
}

bool mockSendACT_TERM(IMasterConnection self, CS101_ASDU /*asdu*/)
{
    auto* state = static_cast<MockMasterConnectionState*>(self->object);
    if (state)
    {
        ++state->actTermCalls;
    }
    return true;
}

sIMasterConnection makeMockMasterConnection(MockMasterConnectionState* state)
{
    sIMasterConnection conn{};
    conn.sendASDU = mockSendASDU;
    conn.sendACT_CON = mockSendACT_CON;
    conn.sendACT_TERM = mockSendACT_TERM;
    conn.object = state;
    return conn;
}

} // namespace

// Launches an IEC 60870-5-104 outstation configured with select-before-operate
// enabled and confirms it starts up in SBO mode. Mirrors the style of
// test_fd_server.cpp (launch the binary, sleep, grep the captured output).
TEST_CASE("testing iec60870-5-104 select-before-operate server startup")
{
    exec("bennu-field-device --f ../data/configs/ep/iec60870-5-104-server-sbo.xml >fd-104-sbo.out 2>&1 &");
    exec("sleep .1");
    exec("pkill -f bennu-field-device");
    std::string res("Initialized IEC60870-5-104 server: tcp://127.0.0.1:2404 (select-before-operate enabled)\n");
    CHECK(exec("grep -o -e 'Initialized IEC60870-5-104 server: tcp://127.0.0.1:2404 (select-before-operate enabled)' fd-104-sbo.out") == res);
}

TEST_CASE("server SBO validates output point family, command match, and connection lifecycle")
{
    auto dm = std::make_shared<DataManager>();
    dm->addExternalData<bool>("bin-in", "bin-in");
    dm->addTagToPointMapping("bin-in", "bin-in");
    dm->addExternalData<bool>("bin-out", "bin-out");
    dm->addTagToPointMapping("bin-out", "bin-out");
    dm->addExternalData<double>("ana-in", "ana-in");
    dm->addTagToPointMapping("ana-in", "ana-in");
    dm->addExternalData<double>("ana-out", "ana-out");
    dm->addTagToPointMapping("ana-out", "ana-out");

    auto server = std::make_shared<Server>(dm);
    REQUIRE(server->addBinaryInput(100, "bin-in"));
    REQUIRE(server->addBinaryOutput(1, "bin-out"));
    REQUIRE(server->addAnalogInput(102, "ana-in"));
    REQUIRE(server->addAnalogOutput(2, "ana-out"));

    auto conn1 = reinterpret_cast<IMasterConnection>(0x1001);
    auto conn2 = reinterpret_cast<IMasterConnection>(0x1002);

    // Unknown IOAs, input points, and wrong command families must be rejected
    CHECK_FALSE(server->select(999, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->select(100, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->select(102, conn1, C_SE_NC_1, 0, 42.0f, 0));
    CHECK_FALSE(server->select(2, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->select(1, conn1, C_SE_NC_1, 0, 42.0f, 0));

    // Prohibited C_DC_NA_1 states (0 = intermediate, 3 = indeterminate) must be rejected
    CHECK_FALSE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_INTERMEDIATE, 0.0f, 0));
    CHECK_FALSE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_INDETERMINATE, 0.0f, 0));

    // Competing SELECT from a different connection while unexpired must be rejected
    // and must not overwrite the original connection's selection
    REQUIRE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->select(1, conn2, C_DC_NA_1, IEC60870_DOUBLE_POINT_OFF, 0.0f, 0));
    CHECK(server->checkAndConsumeSelect(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));

    // Cancellation (COT_DEACTIVATION) only succeeds for the owning connection and valid output point
    REQUIRE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->cancelSelect(100, conn1, C_DC_NA_1));
    CHECK_FALSE(server->cancelSelect(1, conn2, C_DC_NA_1));
    CHECK(server->cancelSelect(1, conn1, C_DC_NA_1));
    CHECK_FALSE(server->cancelSelect(1, conn1, C_DC_NA_1));
    CHECK_FALSE(server->checkAndConsumeSelect(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));

    // Value mismatch on EXECUTE must fail and consume the selection
    REQUIRE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_OFF, 0.0f, 0));
    CHECK_FALSE(server->checkAndConsumeSelect(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->checkAndConsumeSelect(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_OFF, 0.0f, 0));

    // Command type mismatch (e.g. C_SC_NA_1 after C_DC_NA_1 select) must fail
    REQUIRE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->checkAndConsumeSelect(1, conn1, C_SC_NA_1, 1, 0.0f, 0));

    // Qualifier mismatch must fail
    REQUIRE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 1));
    CHECK_FALSE(server->checkAndConsumeSelect(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 2));

    // Different connection must fail
    REQUIRE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->checkAndConsumeSelect(1, conn2, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));

    // Matching command type, value, qualifier, and connection succeeds once
    REQUIRE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK(server->checkAndConsumeSelect(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK_FALSE(server->checkAndConsumeSelect(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));

    // Analog setpoint selection and matching execute
    REQUIRE(server->select(2, conn1, C_SE_NC_1, 0, 12.5f, 0));
    CHECK_FALSE(server->checkAndConsumeSelect(2, conn1, C_SE_NC_1, 0, 99.0f, 0));
    REQUIRE(server->select(2, conn1, C_SE_NC_1, 0, 12.5f, 0));
    CHECK(server->checkAndConsumeSelect(2, conn1, C_SE_NC_1, 0, 12.5f, 0));

    // Connection deactivation/close clears selections for that connection only
    REQUIRE(server->select(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    REQUIRE(server->select(2, conn2, C_SE_NC_1, 0, 5.0f, 0));
    Server::connectionEventHandler(server.get(), conn1, CS104_CON_EVENT_DEACTIVATED);
    CHECK_FALSE(server->checkAndConsumeSelect(1, conn1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CHECK(server->checkAndConsumeSelect(2, conn2, C_SE_NC_1, 0, 5.0f, 0));
}

TEST_CASE("server asduHandler enforces direct-operate, prohibited states, and deactivation ownership")
{
    auto dm = std::make_shared<DataManager>();
    dm->addExternalData<bool>("bin-in", "bin-in");
    dm->addTagToPointMapping("bin-in", "bin-in");
    dm->addExternalData<bool>("bin-out", "bin-out");
    dm->addTagToPointMapping("bin-out", "bin-out");

    auto server = std::make_shared<Server>(dm);
    REQUIRE(server->addBinaryInput(100, "bin-in"));
    REQUIRE(server->addBinaryOutput(1, "bin-out"));

    CS104_Slave slave = CS104_Slave_create(10, 10);
    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    MockMasterConnectionState state1;
    MockMasterConnectionState state2;
    sIMasterConnection mock1 = makeMockMasterConnection(&state1);
    sIMasterConnection mock2 = makeMockMasterConnection(&state2);

    // 1. When SBO is disabled (default), a SELECT (isSelect=true) must be rejected
    // with negative ACT_CON and must NOT write to the point.
    CS101_ASDU selAsdu = CS101_ASDU_create(alParams, false, CS101_COT_ACTIVATION, 0, 1, false, false);
    InformationObject selIo = (InformationObject)DoubleCommand_create(
        nullptr, 1, IEC60870_DOUBLE_POINT_ON, true, 0);
    CS101_ASDU_addInformationObject(selAsdu, selIo);
    InformationObject_destroy(selIo);
    CHECK(Server::asduHandler(server.get(), &mock1, selAsdu));
    CS101_ASDU_destroy(selAsdu);
    CHECK(state1.actConCalls == 1);
    CHECK(state1.lastActConNegative == true);
    CHECK(state1.actTermCalls == 0);
    CHECK(dm->getDataByTag<bool>("bin-out") == false);

    // 2. Direct-operate EXECUTE on an input point (IOA 100) must be rejected with negative ACT_CON.
    CS101_ASDU inExecAsdu = CS101_ASDU_create(alParams, false, CS101_COT_ACTIVATION, 0, 1, false, false);
    InformationObject inExecIo = (InformationObject)DoubleCommand_create(
        nullptr, 100, IEC60870_DOUBLE_POINT_ON, false, 0);
    CS101_ASDU_addInformationObject(inExecAsdu, inExecIo);
    InformationObject_destroy(inExecIo);
    CHECK(Server::asduHandler(server.get(), &mock1, inExecAsdu));
    CS101_ASDU_destroy(inExecAsdu);
    CHECK(state1.actConCalls == 2);
    CHECK(state1.lastActConNegative == true);
    CHECK(state1.actTermCalls == 0);

    // 3. Direct-operate EXECUTE with prohibited DCS state 0 (intermediate) must be rejected.
    CS101_ASDU badStateAsdu = CS101_ASDU_create(alParams, false, CS101_COT_ACTIVATION, 0, 1, false, false);
    InformationObject badStateIo = (InformationObject)DoubleCommand_create(
        nullptr, 1, IEC60870_DOUBLE_POINT_INTERMEDIATE, false, 0);
    CS101_ASDU_addInformationObject(badStateAsdu, badStateIo);
    InformationObject_destroy(badStateIo);
    CHECK(Server::asduHandler(server.get(), &mock1, badStateAsdu));
    CS101_ASDU_destroy(badStateAsdu);
    CHECK(state1.actConCalls == 3);
    CHECK(state1.lastActConNegative == true);
    CHECK(state1.actTermCalls == 0);

    // 4. COT_DEACTIVATION on a selection owned by mock1 is rejected when sent by mock2
    // (negative DEACTIVATION_CON), then succeeds when sent by mock1 (positive DEACTIVATION_CON).
    REQUIRE(server->select(1, &mock1, C_DC_NA_1, IEC60870_DOUBLE_POINT_ON, 0.0f, 0));
    CS101_ASDU deactAsdu2 = CS101_ASDU_create(alParams, false, CS101_COT_DEACTIVATION, 0, 1, false, false);
    InformationObject deactIo2 = (InformationObject)DoubleCommand_create(
        nullptr, 1, IEC60870_DOUBLE_POINT_ON, true, 0);
    CS101_ASDU_addInformationObject(deactAsdu2, deactIo2);
    InformationObject_destroy(deactIo2);
    CHECK(Server::asduHandler(server.get(), &mock2, deactAsdu2));
    CS101_ASDU_destroy(deactAsdu2);
    CHECK(state2.sendAsduCalls == 1);
    CHECK(state2.lastSentCot == CS101_COT_DEACTIVATION_CON);
    CHECK(state2.lastSentNegative == true);

    CS101_ASDU deactAsdu1 = CS101_ASDU_create(alParams, false, CS101_COT_DEACTIVATION, 0, 1, false, false);
    InformationObject deactIo1 = (InformationObject)DoubleCommand_create(
        nullptr, 1, IEC60870_DOUBLE_POINT_ON, true, 0);
    CS101_ASDU_addInformationObject(deactAsdu1, deactIo1);
    InformationObject_destroy(deactIo1);
    CHECK(Server::asduHandler(server.get(), &mock1, deactAsdu1));
    CS101_ASDU_destroy(deactAsdu1);
    CHECK(state1.sendAsduCalls == 1);
    CHECK(state1.lastSentCot == CS101_COT_DEACTIVATION_CON);
    CHECK(state1.lastSentNegative == false);

    // 5. Direct-operate EXECUTE on output point with C_SC_NA_1 sends both ACT_CON and ACT_TERM.
    CS101_ASDU singleExecAsdu = CS101_ASDU_create(alParams, false, CS101_COT_ACTIVATION, 0, 1, false, false);
    InformationObject singleExecIo = (InformationObject)SingleCommand_create(
        nullptr, 1, true, false, 0);
    CS101_ASDU_addInformationObject(singleExecAsdu, singleExecIo);
    InformationObject_destroy(singleExecIo);
    CHECK(Server::asduHandler(server.get(), &mock1, singleExecAsdu));
    CS101_ASDU_destroy(singleExecAsdu);
    CHECK(state1.actConCalls == 4);
    CHECK(state1.lastActConNegative == false);
    CHECK(state1.actTermCalls == 1);
    CHECK(dm->snapshotUpdatedBinaryTags().at("bin-out") == true);

    CS104_Slave_destroy(slave);
}

TEST_CASE("client SBO callback routing, pending write protection, and select-bit checking")
{
    CS104_Slave slave = CS104_Slave_create(10, 10);
    CS104_Slave_setLocalAddress(slave, "127.0.0.1");
    CS104_Slave_setLocalPort(slave, 24104);
    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setASDUHandler(slave, holdSelectAsduHandler, nullptr);
    CS104_Slave_start(slave);
    REQUIRE(CS104_Slave_isRunning(slave));

    auto conn1 = std::make_shared<ClientConnection>("tcp://127.0.0.1:24104", true);
    auto conn2 = std::make_shared<ClientConnection>("tcp://127.0.0.1:24105", true);

    RegisterDescriptor rdBin;
    rdBin.mRegisterType = eStatusReadWrite;
    rdBin.mRegisterAddress = 10;
    rdBin.mTag = "bin-out";
    rdBin.mStatus = false;

    RegisterDescriptor rdAna;
    rdAna.mRegisterType = eValueReadWrite;
    rdAna.mRegisterAddress = 20;
    rdAna.mTag = "ana-out";
    rdAna.mFloatValue = 0.0f;

    conn1->addBinary("bin-out", rdBin);
    conn1->addAnalog("ana-out", rdAna);
    conn2->addBinary("bin-out", rdBin);
    conn2->addAnalog("ana-out", rdAna);

    conn1->start(conn1);
    // Point global gClientConnection at conn2 to verify callbacks use the
    // receiving connection instance rather than gClientConnection.
    gClientConnection = conn2;

    // 1. First write queues SELECT; second write to same IOA before ACT_CON is rejected
    CHECK(conn1->writeBinary("bin-out", true).status == STATUS_SUCCESS);
    CHECK(conn1->writeBinary("bin-out", false).status == STATUS_FAIL);

    CHECK(conn1->writeAnalog("ana-out", 42.5).status == STATUS_SUCCESS);
    CHECK(conn1->writeAnalog("ana-out", 99.0).status == STATUS_FAIL);

    // 2. An EXECUTE ACT_CON (selectCommand=false) must NOT advance or erase a pending SELECT
    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);
    CS101_ASDU execNegAsdu = CS101_ASDU_create(alParams, false, CS101_COT_ACTIVATION_CON, 0, 1, false, true);
    InformationObject execDc = (InformationObject)DoubleCommand_create(
        nullptr, 10, IEC60870_DOUBLE_POINT_OFF, false, 0);
    CS101_ASDU_addInformationObject(execNegAsdu, execDc);
    InformationObject_destroy(execDc);
    ClientConnection::asduReceivedHandler(conn1.get(), 0, execNegAsdu);
    CS101_ASDU_destroy(execNegAsdu);

    // Pending SELECT for IOA 10 must still be present (so another write is still rejected)
    CHECK(conn1->writeBinary("bin-out", false).status == STATUS_FAIL);

    // 3. A positive SELECT ACT_CON (selectCommand=true) executes the original command
    // on conn1 (not conn2) and clears the pending entry
    CS101_ASDU selPosAsdu = CS101_ASDU_create(alParams, false, CS101_COT_ACTIVATION_CON, 0, 1, false, false);
    InformationObject selDc = (InformationObject)DoubleCommand_create(
        nullptr, 10, IEC60870_DOUBLE_POINT_ON, true, 0);
    CS101_ASDU_addInformationObject(selPosAsdu, selDc);
    InformationObject_destroy(selDc);
    ClientConnection::asduReceivedHandler(conn1.get(), 0, selPosAsdu);
    CS101_ASDU_destroy(selPosAsdu);

    RegisterDescriptor out1;
    RegisterDescriptor out2;
    REQUIRE(conn1->getRegisterDescriptorByTag("bin-out", out1));
    REQUIRE(conn2->getRegisterDescriptorByTag("bin-out", out2));
    CHECK(out1.mStatus == true);
    CHECK(out2.mStatus == false);

    // Now that the pending SELECT was consumed by EXECUTE, a new write on IOA 10 is accepted
    CHECK(conn1->writeBinary("bin-out", false).status == STATUS_SUCCESS);

    // 4. A positive SELECT ACT_CON that echoes a mismatched command state (ON instead of OFF)
    // must abort EXECUTE (leaving out1.mStatus unchanged at true) and clear the pending entry.
    CS101_ASDU selMismatchAsdu = CS101_ASDU_create(alParams, false, CS101_COT_ACTIVATION_CON, 0, 1, false, false);
    InformationObject selMismatchDc = (InformationObject)DoubleCommand_create(
        nullptr, 10, IEC60870_DOUBLE_POINT_ON, true, 0);
    CS101_ASDU_addInformationObject(selMismatchAsdu, selMismatchDc);
    InformationObject_destroy(selMismatchDc);
    ClientConnection::asduReceivedHandler(conn1.get(), 0, selMismatchAsdu);
    CS101_ASDU_destroy(selMismatchAsdu);

    REQUIRE(conn1->getRegisterDescriptorByTag("bin-out", out1));
    CHECK(out1.mStatus == true);
    CHECK(conn1->writeBinary("bin-out", false).status == STATUS_SUCCESS);

    CS104_Slave_stop(slave);
    CS104_Slave_destroy(slave);
}
