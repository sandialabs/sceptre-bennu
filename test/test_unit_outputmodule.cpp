#include "doctest.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <memory>

#include "bennu/devices/modules/io/OutputModule.hpp"

using bennu::field_device::DataManager;

namespace {

// OutputModule with the points it "owns" registered directly
struct TestModule : bennu::io::OutputModule
{
    void own(const std::string& point)
    {
        onPoint(point);
    }
};

enum Reply
{
    ACK,
    ERR,
    NONE
}; // NONE = provider never answered

struct Fixture
{
    std::shared_ptr<DataManager> dm = std::make_shared<DataManager>();
    TestModule om;
    std::deque<Reply> script; // one entry per send, consumed in order
    std::vector<std::string> sent;
    Reply last = ACK;
    std::function<void()> onSend;

    Fixture()
    {
        om.setDataManager(dm);
        om.setSender(
            [this](const std::string& p) {
                sent.push_back(p);
                last = script.empty() ? ACK : script.front();
                if (!script.empty())
                {
                    script.pop_front();
                }
                if (onSend)
                {
                    onSend();
                }
                return last == ACK;
            },
            [this]() { return last != NONE; });
    }

    // tag "t_<point>" -> point <point>; owned by this module unless owned == false
    template <typename T>
    void add(const std::string& point, bool owned = true)
    {
        dm->addExternalData<T>("id_" + point, point);
        dm->addTagToPointMapping("t_" + point, "id_" + point);
        if (owned)
        {
            om.own(point);
        }
    }
};

} // namespace

TEST_CASE("batched success sends once, applies values and erases")
{
    Fixture f;
    f.add<bool>("b");
    f.add<double>("a");
    f.dm->addUpdatedBinaryTag("t_b", true);
    f.dm->addUpdatedAnalogTag("t_a", 2.5);

    f.om.scanOutputs();

    CHECK(f.sent.size() == 1);
    CHECK(f.dm->getDataByTag<bool>("t_b") == true);
    CHECK(f.dm->getDataByTag<double>("t_a") == 2.5);
    CHECK(f.dm->snapshotUpdatedBinaryTags().empty());
    CHECK(f.dm->snapshotUpdatedAnalogTags().empty());
}

TEST_CASE("value written during send survives the erase")
{
    Fixture f;
    f.add<bool>("b");
    f.add<double>("a");
    f.dm->addUpdatedBinaryTag("t_b", true);
    f.dm->addUpdatedAnalogTag("t_a", 1.0);
    f.onSend = [&] {
        f.dm->addUpdatedBinaryTag("t_b", false);
        f.dm->addUpdatedAnalogTag("t_a", 9.0);
    };

    f.om.scanOutputs();

    CHECK(f.dm->snapshotUpdatedBinaryTags().at("t_b") == false);
    CHECK(f.dm->snapshotUpdatedAnalogTags().at("t_a") == 9.0);
}

TEST_CASE("outage keeps the maps and does not fan out")
{
    Fixture f;
    f.add<bool>("b1");
    f.add<bool>("b2");
    f.add<double>("a");
    f.dm->addUpdatedBinaryTag("t_b1", true);
    f.dm->addUpdatedBinaryTag("t_b2", true);
    f.dm->addUpdatedAnalogTag("t_a", 1.0);
    f.script = {NONE};

    f.om.scanOutputs();

    CHECK(f.sent.size() == 1);
    CHECK(f.dm->snapshotUpdatedBinaryTags().size() == 2);
    CHECK(f.dm->snapshotUpdatedAnalogTags().size() == 1);
    CHECK(f.dm->getDataByTag<bool>("t_b1") == false);
}

TEST_CASE("ERR on a single point drops it without a retry")
{
    Fixture f;
    f.add<double>("a");
    f.dm->addUpdatedAnalogTag("t_a", 1.0);
    f.script = {ERR};

    f.om.scanOutputs();

    CHECK(f.sent.size() == 1);
    CHECK(f.dm->snapshotUpdatedAnalogTags().empty());
}

TEST_CASE("ERR batch fans out per point, rejected points are dropped")
{
    Fixture f;
    f.add<bool>("b1");
    f.add<bool>("b2");
    f.add<double>("a");
    f.dm->addUpdatedBinaryTag("t_b1", true);
    f.dm->addUpdatedBinaryTag("t_b2", true);
    f.dm->addUpdatedAnalogTag("t_a", 4.0);
    f.script = {ERR, ACK, ERR, ACK}; // batch, b1, b2, a

    f.om.scanOutputs();

    REQUIRE(f.sent.size() == 4);
    CHECK(f.sent[1] == "b1:true");
    CHECK(f.sent[2] == "b2:true");
    CHECK(f.dm->getDataByTag<bool>("t_b1") == true);
    CHECK(f.dm->getDataByTag<double>("t_a") == 4.0);
    CHECK(f.dm->snapshotUpdatedBinaryTags().empty()); // b2 dropped, not retried
    CHECK(f.dm->snapshotUpdatedAnalogTags().empty());
}

TEST_CASE("reply lost mid fan-out is an outage")
{
    Fixture f;
    f.add<bool>("b1");
    f.add<bool>("b2");
    f.add<double>("a");
    f.dm->addUpdatedBinaryTag("t_b1", true);
    f.dm->addUpdatedBinaryTag("t_b2", true);
    f.dm->addUpdatedAnalogTag("t_a", 4.0);
    f.script = {ERR, ACK, NONE}; // batch, b1, b2 (no reply)

    f.om.scanOutputs();

    CHECK(f.sent.size() == 3);                            // analog never attempted
    CHECK(f.dm->snapshotUpdatedBinaryTags().size() == 2); // nothing erased, even acked b1
    CHECK(f.dm->snapshotUpdatedAnalogTags().size() == 1);
    CHECK(f.dm->getDataByTag<bool>("t_b1") == false);
}

TEST_CASE("other modules' points and internal tags are not sent")
{
    Fixture f;
    f.add<bool>("mine");
    f.add<bool>("theirs", false);
    f.dm->addOutputPoint("theirs"); // owned by another OutputModule
    f.dm->addInternalData<bool>("internal", false);
    f.dm->addUpdatedBinaryTag("t_mine", true);
    f.dm->addUpdatedBinaryTag("t_theirs", true);
    f.dm->addUpdatedBinaryTag("internal", true);

    f.om.scanOutputs();

    REQUIRE(f.sent.size() == 1);
    CHECK(f.sent[0] == "mine:true");
    auto left = f.dm->snapshotUpdatedBinaryTags();
    CHECK(left.size() == 2);
    CHECK(left.count("t_theirs") == 1);
    CHECK(left.count("internal") == 1);
}

TEST_CASE("points no output module owns are still sent and cleared")
{
    Fixture f;
    f.add<bool>("mine");
    f.add<bool>("input_only", false); // e.g. an input point a comms master or logic writes
    f.dm->addUpdatedBinaryTag("t_mine", true);
    f.dm->addUpdatedBinaryTag("t_input_only", true);

    f.om.scanOutputs();

    REQUIRE(f.sent.size() == 1);
    CHECK(f.sent[0] == "input_only:true,mine:true");
    CHECK(f.dm->snapshotUpdatedBinaryTags().empty());
}

TEST_CASE("perf: 800 changed tags go out in one request")
{
    Fixture f;
    for (int i = 0; i < 800; ++i)
    {
        f.add<double>("p" + std::to_string(i));
        f.dm->addUpdatedAnalogTag("t_p" + std::to_string(i), i);
    }

    f.om.scanOutputs();

    REQUIRE(f.sent.size() == 1); // one REQ per cycle, never one per tag
    CHECK(std::count(f.sent[0].begin(), f.sent[0].end(), ',') == 799);
    CHECK(f.dm->snapshotUpdatedAnalogTags().empty());
}
