#include "doctest.h"

#include <cstring>
#include <memory>
#include <string>

#include "bennu/devices/modules/comms/base/CommandInterface.hpp"
#include "bennu/devices/modules/comms/base/CommsClient.hpp"

using namespace bennu::comms;

namespace {

class MockClient : public CommsClient
{
public:
    std::set<std::string> getTags() const override
    {
        return {"load-mw-setpoint", "load-breaker-toggle"};
    }
    bool isValidTag(const std::string& tag) const override
    {
        return getTags().count(tag) > 0;
    }
    StatusMessage readTag(const std::string& tag, RegisterDescriptor& rd) const override
    {
        rd.mRegisterType = eStatusReadWrite;
        rd.mStatus = true;
        return {STATUS_SUCCESS, nullptr};
    }
    StatusMessage writeBinaryTag(const std::string&, bool) override
    {
        return {STATUS_SUCCESS, nullptr};
    }
    StatusMessage writeAnalogTag(const std::string&, double v) override
    {
        lastAnalog = v;
        return {STATUS_SUCCESS, nullptr};
    }
    double lastAnalog = 0;
};

class TestInterface : public CommandInterface
{
public:
    using CommandInterface::CommandInterface;
    using CommandInterface::messageHandler;
};

std::string reply(TestInterface& ci, const zmq::message_t& req)
{
    zmq::message_t rep = ci.messageHandler(req);
    return std::string(rep.data<char>());
}

} // namespace

TEST_CASE("CommandInterface: WRITE with a non-numeric value returns ERR instead of throwing")
{
    auto client = std::make_shared<MockClient>();
    TestInterface ci(bennu::distributed::Endpoint{"inproc://unit-cmdiface-stod"}, client);

    std::string req = "WRITE=load-mw-setpoint:abc";
    std::string rep;
    CHECK_NOTHROW(rep = reply(ci, zmq::message_t(req.c_str(), req.size() + 1)));
    CHECK(rep.rfind("ERR=", 0) == 0);

    req = "WRITE=load-mw-setpoint:12.5";
    CHECK(reply(ci, zmq::message_t(req.c_str(), req.size() + 1)).rfind("ACK=", 0) == 0);
    CHECK(client->lastAnalog == 12.5);
}
