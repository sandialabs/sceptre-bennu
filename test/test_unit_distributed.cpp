#include "doctest.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bennu/distributed/Client.hpp"
#include "bennu/distributed/Provider.hpp"
#include "bennu/distributed/Publisher.hpp"
#include "bennu/distributed/Server.hpp"
#include "bennu/distributed/Subscriber.hpp"
#include "bennu/distributed/Utils.hpp"

using namespace bennu::distributed;

namespace {

inline zmq::message_t makeReq(const std::string& str)
{
    return zmq::message_t(str.c_str(), str.length() + 1);
}

static std::atomic<int> sProvCounter{0};

class MockProvider : public Provider
{
public:
    MockProvider() :
        Provider(Endpoint{"inproc://mock-prov-" + std::to_string(++sProvCounter)},
                 Endpoint{"udp://127.0.0.1:" + std::to_string(45500 + (sProvCounter % 1000))})
    {}

    using Provider::messageHandler;

    std::string mockQueryReply{"ACK=tag1,tag2"};
    std::string mockReadReply{"ACK=tag1:100"};
    std::string mockWriteReply{"ACK=ok"};

    std::string lastReadTag;
    std::unordered_map<std::string, std::string> lastWrittenTags;
    int queryCalls = 0;
    int readCalls = 0;
    int writeCalls = 0;

    std::string query() override
    {
        queryCalls++;
        return mockQueryReply;
    }

    std::string read(const std::string& tag) override
    {
        readCalls++;
        lastReadTag = tag;
        return mockReadReply;
    }

    std::string write(const std::unordered_map<std::string, std::string>& tags) override
    {
        writeCalls++;
        lastWrittenTags = tags;
        return mockWriteReply;
    }

    [[noreturn]] void periodicPublish() override
    {
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};

} // namespace

TEST_CASE("Provider messageHandler: QUERY / query case-insensitivity")
{
    MockProvider mock;
    mock.mockQueryReply = "ACK=tag1,tag2";

    // Upper case
    zmq::message_t req1 = makeReq("QUERY=");
    zmq::message_t rep1 = mock.messageHandler(req1);
    CHECK(std::string(rep1.data<char>()) == "ACK=tag1,tag2");
    CHECK(mock.queryCalls == 1);

    // Lower case
    zmq::message_t req2 = makeReq("query=");
    zmq::message_t rep2 = mock.messageHandler(req2);
    CHECK(std::string(rep2.data<char>()) == "ACK=tag1,tag2");
    CHECK(mock.queryCalls == 2);
}

TEST_CASE("Provider messageHandler: READ success and error")
{
    MockProvider mock;

    // Successful read
    mock.mockReadReply = "ACK=voltage:120.5";
    zmq::message_t req1 = makeReq("READ=voltage");
    zmq::message_t rep1 = mock.messageHandler(req1);
    CHECK(std::string(rep1.data<char>()) == "ACK=voltage:120.5");
    CHECK(mock.lastReadTag == "voltage");
    CHECK(mock.readCalls == 1);

    // Error read
    mock.mockReadReply = "ERR=Tag not found";
    zmq::message_t req2 = makeReq("read=unknown_tag");
    zmq::message_t rep2 = mock.messageHandler(req2);
    CHECK(std::string(rep2.data<char>()) == "ERR=Tag not found");
    CHECK(mock.lastReadTag == "unknown_tag");
    CHECK(mock.readCalls == 2);
}

TEST_CASE("Provider messageHandler: WRITE single and batch updates")
{
    MockProvider mock;

    // Single point write
    zmq::message_t req1 = makeReq("WRITE=speed:3500");
    zmq::message_t rep1 = mock.messageHandler(req1);
    CHECK(std::string(rep1.data<char>()) == "ACK=ok");
    CHECK(mock.writeCalls == 1);
    CHECK(mock.lastWrittenTags.size() == 1);
    CHECK(mock.lastWrittenTags["speed"] == "3500");

    // Multi-point comma-separated batch
    zmq::message_t req2 = makeReq("write=temp:72.5,pressure:101.3,valve:true");
    zmq::message_t rep2 = mock.messageHandler(req2);
    CHECK(std::string(rep2.data<char>()) == "ACK=ok");
    CHECK(mock.writeCalls == 2);
    CHECK(mock.lastWrittenTags.size() == 3);
    CHECK(mock.lastWrittenTags["temp"] == "72.5");
    CHECK(mock.lastWrittenTags["pressure"] == "101.3");
    CHECK(mock.lastWrittenTags["valve"] == "true");
}

TEST_CASE("Provider messageHandler: key-value parsing robustness")
{
    MockProvider mock;

    // Tag values containing '=': Provider splits op/payload on '=' then tags on ':'
    zmq::message_t reqEq = makeReq("WRITE=config:a=b");
    zmq::message_t repEq = mock.messageHandler(reqEq);
    CHECK(std::string(repEq.data<char>()) == "ACK=ok");
    CHECK(mock.lastWrittenTags["config"] == "a");

    // Tag values containing colons
    zmq::message_t reqColon = makeReq("WRITE=url:http://localhost");
    zmq::message_t repColon = mock.messageHandler(reqColon);
    CHECK(std::string(repColon.data<char>()) == "ACK=ok");
    CHECK(mock.lastWrittenTags["url"] == "http");

    // Empty tag value
    zmq::message_t reqEmptyVal = makeReq("WRITE=empty_val:");
    zmq::message_t repEmptyVal = mock.messageHandler(reqEmptyVal);
    CHECK(std::string(repEmptyVal.data<char>()) == "ACK=ok");
    CHECK(mock.lastWrittenTags["empty_val"] == "");

    // Trailing comma
    mock.lastWrittenTags.clear();
    zmq::message_t reqTrailing = makeReq("WRITE=p1:10,p2:20,");
    zmq::message_t repTrailing = mock.messageHandler(reqTrailing);
    CHECK(std::string(repTrailing.data<char>()) == "ACK=ok");
    CHECK(mock.lastWrittenTags.size() == 2);
    CHECK(mock.lastWrittenTags["p1"] == "10");
    CHECK(mock.lastWrittenTags["p2"] == "20");
}

TEST_CASE("Provider messageHandler: error responses")
{
    MockProvider mock;

    // Empty request
    zmq::message_t reqEmpty = makeReq("");
    zmq::message_t repEmpty = mock.messageHandler(reqEmpty);
    CHECK(std::string(repEmpty.data<char>()) == "ERR=Unknown command type ''");

    // Unknown command
    zmq::message_t reqUnknown = makeReq("UNKNOWN=123");
    zmq::message_t repUnknown = mock.messageHandler(reqUnknown);
    CHECK(std::string(repUnknown.data<char>()) == "ERR=Unknown command type 'UNKNOWN'");

    // Invalid syntax without '=' delimiter
    zmq::message_t reqNoDelim = makeReq("INVALID_CMD");
    zmq::message_t repNoDelim = mock.messageHandler(reqNoDelim);
    CHECK(std::string(repNoDelim.data<char>()) == "ERR=Unknown command type 'INVALID_CMD'");

    // Empty delimiter split does not hang and returns full phrase
    auto singlePart = split("hello_world", "");
    REQUIRE(singlePart.size() == 1);
    CHECK(singlePart[0] == "hello_world");
}

TEST_CASE("Server & Client In-Process REQ/REP over inproc://")
{
    const std::string inprocEndpoint = "inproc://unit-dist-reqrep";

    Server server(Endpoint{inprocEndpoint});
    server.setHandler([&server](const zmq::message_t& req) -> zmq::message_t {
        std::string s(req.data<char>());
        if (s == "SHUTDOWN")
        {
            server.stop();
            return makeReq("ACK=bye");
        }
        if (s.find("ack_test") != std::string::npos)
        {
            return makeReq("ACK=success_payload");
        }
        if (s.find("err_test") != std::string::npos)
        {
            return makeReq("ERR=failure_reason");
        }
        if (s.find("malformed_test") != std::string::npos)
        {
            return makeReq("MALFORMED_NO_DELIM");
        }
        return makeReq("ACK=ok");
    });

    std::thread serverThread([&server]() {
        server.run();
    });

    struct ServerThreadGuard
    {
        std::thread& t;
        Server& s;
        std::string ep;
        ~ServerThreadGuard()
        {
            if (t.joinable())
            {
                s.stop();
                try
                {
                    Client c(Endpoint{ep});
                    c.send("SHUTDOWN");
                }
                catch (...)
                {
                }
                t.join();
            }
        }
    } threadGuard{serverThread, server, inprocEndpoint};

    Client client(Endpoint{inprocEndpoint});

    // Check initial replied() state
    CHECK_FALSE(client.replied());

    // Test ACK handling: handler called, send returns true, replied() is true
    bool handlerCalled = false;
    std::string receivedData;
    client.setHandler([&](const std::string& reply) {
        handlerCalled = true;
        receivedData = reply;
    });

    bool ackResult = client.send("WRITE=ack_test:1");
    CHECK(ackResult);
    CHECK(handlerCalled);
    CHECK(receivedData == "success_payload");
    CHECK(client.replied());

    // Test ERR handling: handler not called, send returns false, replied() is true
    handlerCalled = false;
    bool errResult = client.send("WRITE=err_test:1");
    CHECK_FALSE(errResult);
    CHECK_FALSE(handlerCalled);
    CHECK(client.replied());

    // Test Malformed replies: send returns false
    handlerCalled = false;
    bool malResult = client.send("WRITE=malformed_test:1");
    CHECK_FALSE(malResult);
    CHECK_FALSE(handlerCalled);
    CHECK(client.replied());

    // Test writePoint(double), writePoint(bool), writePoints(payload)
    client.writePoint("analog1", 42.5);
    CHECK(client.replied());

    client.writePoint("bin1", true);
    CHECK(client.replied());

    client.writePoint("bin0", false);
    CHECK(client.replied());

    bool wpResult = client.writePoints("p1:1.0,p2:true");
    CHECK(wpResult);
    CHECK(client.replied());

    // Clean shutdown of server
    bool shutResult = client.send("SHUTDOWN");
    CHECK(shutResult);
    serverThread.join();
}

TEST_CASE("Endpoint Group Hashing")
{
    // Interface removal: udp://eth0;239.0.0.1:40000 stripped to 239.0.0.1:40000
    Endpoint epWithIface{"udp://eth0;239.0.0.1:40000"};
    Endpoint epWithoutIface{"udp://239.0.0.1:40000"};
    CHECK(epWithIface.hash() == epWithoutIface.hash());

    Endpoint epWlan{"udp://wlan0;192.168.1.1:50000"};
    Endpoint epWlanBare{"udp://192.168.1.1:50000"};
    CHECK(epWlan.hash() == epWlanBare.hash());

    // Hash length invariant: length <= 15 chars (RADIO/DISH max group limit)
    std::vector<std::string> endpoints = {
        "",
        "tcp://127.0.0.1:5555",
        "udp://eth0;239.0.0.1:40000",
        "udp://239.0.0.1:40000",
        "inproc://some-very-long-inproc-endpoint-name-to-test-hashing-bounds:123456",
        "udp://verylonginterfacename;224.0.0.251:5353"};

    for (const auto& epStr : endpoints)
    {
        Endpoint ep{epStr};
        std::string h = ep.hash();
        CHECK(h.length() <= 15);
        // Repeatability: same endpoint produces same hash
        CHECK(ep.hash() == h);
        // Valid hex characters
        CHECK(h.find_first_not_of("0123456789abcdef") == std::string::npos);
    }
}

TEST_CASE("Pub/Sub framing and chunking bounds")
{
    // Link MTU calculation
    unsetenv("BENNU_PUBLISH_MTU");
    CHECK(publishLinkMtu() == 1500);

    // Link MTU error paths
    setenv("BENNU_PUBLISH_MTU", "abc", 1);
    CHECK_THROWS_AS(publishLinkMtu(), std::invalid_argument);
    setenv("BENNU_PUBLISH_MTU", "500", 1); // Below 576 min
    CHECK_THROWS_AS(publishLinkMtu(), std::invalid_argument);
    setenv("BENNU_PUBLISH_MTU", "9999", 1); // Above 8220 max
    CHECK_THROWS_AS(publishLinkMtu(), std::invalid_argument);
    unsetenv("BENNU_PUBLISH_MTU");

    // Invariant: IP_UDP_HEADERS (28) + 1 + groupLength + publishBodyLimit <= linkMtu
    for (size_t groupLen : {0UL, 8UL, 15UL})
    {
        size_t limit = publishBodyLimit(1500, groupLen);
        CHECK(limit == 1500 - 28 - 1 - groupLen);
        CHECK(28 + 1 + groupLen + limit <= 1500);
    }

    // Chunking bounds: message smaller than limit fits in 1 chunk
    size_t limit15 = publishBodyLimit(1500, 15); // 1456
    std::string smallMsg = "tag1:1.0,tag2:2.0";
    auto chunksSmall = chunkMessage(smallMsg, limit15);
    REQUIRE(chunksSmall.size() == 1);
    CHECK(chunksSmall[0] == "tag1:1.0,tag2:2.0,");

    // Large message exceeds limit and is partitioned into multiple chunks <= limit
    std::string largeMsg;
    for (int i = 0; i < 200; ++i)
    {
        largeMsg += "point_" + std::to_string(i) + ":" + std::to_string(i * 1.5) + ",";
    }
    auto chunksLarge = chunkMessage(largeMsg, limit15);
    CHECK(chunksLarge.size() > 1);
    for (const auto& c : chunksLarge)
    {
        CHECK(c.length() < limit15);
    }

    // Localhost mock pub/sub framing over UDP
    Endpoint pubEp{"udp://127.0.0.1:49200"};
    Publisher pub(pubEp);
    std::string testMsg = "bus-1:100.0";
    // Verify publish executes without throwing
    pub.publish(testMsg);
}
