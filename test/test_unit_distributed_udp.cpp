#include "doctest.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "bennu/distributed/Publisher.hpp"
#include "bennu/distributed/Subscriber.hpp"
#include "bennu/distributed/Utils.hpp"

using namespace bennu::distributed;

namespace {

static std::atomic<int> sPortCounter{0};

inline int nextUdpPort()
{
    return 54320 + (sPortCounter.fetch_add(1) % 1000);
}

} // namespace

TEST_CASE("ZMQ RADIO/DISH UDP: Live loopback Pub/Sub communication")
{
    int port = nextUdpPort();
    Endpoint ep{"udp://127.0.0.1:" + std::to_string(port)};

    std::mutex mtx;
    std::condition_variable cv;
    std::string receivedData;
    bool messageReceived = false;

    // Create Subscriber on local UDP loopback
    Subscriber subscriber(ep, [&](std::string& data) {
        std::scoped_lock<std::mutex> lock(mtx);
        receivedData = data;
        messageReceived = true;
        cv.notify_one();
    });

    // Small delay to ensure subscriber thread is bound and joined to the group
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Create Publisher and emit message
    Publisher publisher(ep);
    std::string testMsg = "tag1:100.5,tag2:true,tag3:hello";
    publisher.publish(testMsg);

    // Wait for receipt
    {
        std::unique_lock<std::mutex> lock(mtx);
        bool success = cv.wait_for(lock, std::chrono::seconds(2), [&]() { return messageReceived; });
        CHECK(success);
        CHECK(receivedData == testMsg);
    }
}

TEST_CASE("ZMQ RADIO/DISH UDP: Group filtering invariant")
{
    int port = nextUdpPort();
    Endpoint epSub{"udp://127.0.0.1:" + std::to_string(port)};

    std::mutex mtx;
    std::vector<std::string> receivedMessages;

    // Subscriber joined only to epSub.hash() group
    Subscriber subscriber(epSub, [&](std::string& data) {
        std::scoped_lock<std::mutex> lock(mtx);
        receivedMessages.push_back(data);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Emit datagram with a DIFFERENT group via raw RADIO socket to the same UDP port
    zmq::socket_t foreignRadio(Context::the()->getContext(), ZMQ_RADIO);
    foreignRadio.connect(epSub.str);
    std::string foreignPayload = "foreign_payload_to_ignore\0";
    zmq::message_t foreignMsg(foreignPayload.data(), foreignPayload.size());
    foreignMsg.set_group("otherGroup");
    foreignRadio.send(foreignMsg);

    // Give network stack brief time to deliver / drop
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Now emit datagram matching epSub's group via Publisher
    Publisher publisher(epSub);
    std::string matchingPayload = "legit_matching_message";
    publisher.publish(matchingPayload);

    // Wait for arrival of the matching message
    for (int i = 0; i < 20; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::scoped_lock<std::mutex> lock(mtx);
        if (!receivedMessages.empty())
        {
            break;
        }
    }

    std::scoped_lock<std::mutex> lock(mtx);
    REQUIRE(receivedMessages.size() == 1);
    CHECK(receivedMessages[0] == matchingPayload);
}

TEST_CASE("ZMQ RADIO/DISH UDP: Multi-part datagram chunking exceeding MTU")
{
    int port = nextUdpPort();
    Endpoint ep{"udp://127.0.0.1:" + std::to_string(port)};

    // Construct a large payload exceeding the 1500 byte link MTU (150 tags ~2700 bytes)
    std::string largeMessage;
    const int totalTags = 150;
    for (int i = 0; i < totalTags; ++i)
    {
        largeMessage += "tag_" + std::to_string(i) + ":" + std::to_string(100.0 + i) + ",";
    }

    std::mutex mtx;
    std::condition_variable cv;
    std::map<std::string, std::string> parsedTags;
    int chunkCount = 0;

    Subscriber subscriber(ep, [&](std::string& data) {
        std::scoped_lock<std::mutex> lock(mtx);
        chunkCount++;
        auto points = split(data, ",");
        for (const auto& pt : points)
        {
            if (pt.empty())
            {
                continue;
            }
            auto kv = split(pt, ":");
            if (kv.size() == 2)
            {
                parsedTags[kv[0]] = kv[1];
            }
        }
        if (parsedTags.size() >= totalTags)
        {
            cv.notify_one();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Publisher publisher(ep);
    publisher.publish(largeMessage);

    {
        std::unique_lock<std::mutex> lock(mtx);
        bool success = cv.wait_for(lock, std::chrono::seconds(2), [&]() {
            return parsedTags.size() >= totalTags;
        });
        CHECK(success);
        CHECK(chunkCount > 1); // Confirmed split into multiple UDP chunks
        CHECK(parsedTags.size() == totalTags);
        CHECK(parsedTags["tag_0"] == "100.000000");
        CHECK(parsedTags["tag_149"] == "249.000000");
    }
}

TEST_CASE("ZMQ RADIO/DISH UDP: RAII clean shutdown of subscriber thread")
{
    int port = nextUdpPort();
    Endpoint ep{"udp://127.0.0.1:" + std::to_string(port)};

    // Instantiate and destroy immediately in a localized block
    {
        Subscriber sub(ep, [](std::string&) {});
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // Destruction must complete cleanly with joined thread and closed socket without crashing
    CHECK(true);
}
