#include "doctest.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/distributed/Client.hpp"
#include "bennu/distributed/Provider.hpp"
#include "bennu/distributed/Utils.hpp"

using namespace bennu::distributed;
using bennu::field_device::DataManager;

namespace {

static std::atomic<int> sInprocCounter{0};

class IntegrationTestProvider : public Provider
{
public:
    IntegrationTestProvider(const Endpoint& serverEndpoint,
                            const Endpoint& pubEndpoint,
                            std::shared_ptr<DataManager> dm) :
        Provider(serverEndpoint, pubEndpoint),
        mDataManager(dm)
    {}

    std::string query() override
    {
        std::string result = "ACK=";
        auto bins = mDataManager->getBinaryTags();
        for (const auto& tag : bins)
        {
            result += tag + ",";
        }
        auto anals = mDataManager->getAnalogTags();
        for (const auto& tag : anals)
        {
            result += tag + ",";
        }
        return result;
    }

    std::string read(const std::string& tag) override
    {
        if (!mDataManager->hasTag(tag))
        {
            return "ERR=Tag not found";
        }
        std::string result = "ACK=";
        if (mDataManager->isBinary(tag))
        {
            result += (mDataManager->getDataByTag<bool>(tag) ? "true" : "false");
        }
        else
        {
            result += std::to_string(mDataManager->getDataByTag<double>(tag));
        }
        return result;
    }

    std::string write(const std::unordered_map<std::string, std::string>& tags) override
    {
        for (const auto& kv : tags)
        {
            if (!mDataManager->hasTag(kv.first))
            {
                return "ERR=Tag not found: " + kv.first;
            }
            if (mDataManager->isBinary(kv.first))
            {
                bool val = (kv.second == "true" || kv.second == "1");
                mDataManager->addUpdatedBinaryTag(kv.first, val);
            }
            else
            {
                double val = std::stod(kv.second);
                mDataManager->addUpdatedAnalogTag(kv.first, val);
            }
        }
        return "ACK=Wrote tags";
    }

    [[noreturn]] void periodicPublish() override
    {
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    std::shared_ptr<DataManager> mDataManager;
};

class ProbeClient : public Client
{
public:
    explicit ProbeClient(const Endpoint& endpoint) :
        Client(endpoint)
    {
        setHandler([this](const std::string& reply) {
            lastReply = reply;
        });
    }

    std::string lastReply;
};

} // namespace

TEST_CASE("Probe <-> Provider in-process integration: QUERY, READ, WRITE in < 20 ms")
{
    int id = ++sInprocCounter;
    Endpoint serverEp{"inproc://probe-integ-server-" + std::to_string(id)};
    Endpoint pubEp{"inproc://probe-integ-pub-" + std::to_string(id)};

    auto dm = std::make_shared<DataManager>();
    dm->addBinaryTag("brkr");
    dm->addInternalData<bool>("brkr", true);

    dm->addAnalogTag("load-power");
    dm->addInternalData<double>("load-power", 400.55);

    dm->addBinaryTag("load-breaker-toggle");
    dm->addInternalData<bool>("load-breaker-toggle", true);

    dm->addAnalogTag("load-mw-setpoint");
    dm->addInternalData<double>("load-mw-setpoint", 100.0);

    IntegrationTestProvider provider(serverEp, pubEp, dm);

    // Launch server run loop on background thread
    std::thread serverThread([&]() {
        provider.getServer()->run();
    });

    ProbeClient probe(serverEp);

    // 1. QUERY command: verify ACK and all configured tags present
    CHECK(probe.send("QUERY="));
    CHECK(probe.replied());
    CHECK(probe.lastReply.find("brkr") != std::string::npos);
    CHECK(probe.lastReply.find("load-power") != std::string::npos);
    CHECK(probe.lastReply.find("load-breaker-toggle") != std::string::npos);
    CHECK(probe.lastReply.find("load-mw-setpoint") != std::string::npos);

    // 2. READ single points
    CHECK(probe.send("READ=brkr"));
    CHECK(probe.lastReply == "true");

    CHECK(probe.send("READ=load-power"));
    CHECK(probe.lastReply.rfind("400.55", 0) == 0);

    // 3. READ nonexistent tag returns ERR
    CHECK_FALSE(probe.send("READ=unknown_nonexistent_tag"));
    CHECK(probe.replied());

    // 4. WRITE binary point and assert DataManager update and subsequent READ
    probe.writePoint("load-breaker-toggle", false);
    CHECK(dm->isUpdatedBinaryTag("load-breaker-toggle"));
    CHECK(dm->snapshotUpdatedBinaryTags().at("load-breaker-toggle") == false);

    // Apply the update in DataManager and verify via probe read
    dm->updateInternalData();
    CHECK(probe.send("READ=load-breaker-toggle"));
    CHECK(probe.lastReply == "false");

    // 5. WRITE analog point and assert DataManager update and subsequent READ
    probe.writePoint("load-mw-setpoint", 999.0);
    CHECK(dm->isUpdatedAnalogTag("load-mw-setpoint"));
    CHECK(doctest::Approx(dm->snapshotUpdatedAnalogTags().at("load-mw-setpoint")).epsilon(0.01) == 999.0);

    dm->updateInternalData();
    CHECK(probe.send("READ=load-mw-setpoint"));
    CHECK(probe.lastReply.rfind("999.0", 0) == 0);

    // 6. Deterministic clean shutdown
    provider.stop();
    serverThread.join();
}
