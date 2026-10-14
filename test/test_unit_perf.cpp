// Performance regression guards. These check invariants and loose time bounds, not exact
// timings, so they stay stable on slow CI runners while still catching the bugs that made an
// 800-tag cycle take ~400 s (a fixed sleep per request and one request per tag).
#include "doctest.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include "bennu/distributed/Client.hpp"
#include "bennu/distributed/Utils.hpp"

using namespace bennu::distributed;

TEST_CASE("perf: Client::send has no fixed per-request delay")
{
    setenv("BENNU_LOG_LEVEL", "ERROR", 1); // keep 100 "I: ACK" lines out of the test output
    const int n = 100;
    zmq::socket_t rep(Context::the()->getContext(), ZMQ_REP);
    rep.bind("inproc://perf-client");
    std::thread server([&rep] {
        for (int i = 0; i < n; ++i)
        {
            zmq::message_t req;
            rep.recv(&req);
            std::string ack("ACK=ok");
            rep.send(zmq::message_t(ack.c_str(), ack.size() + 1));
        }
    });

    Client client(Endpoint{"inproc://perf-client"});
    auto start = std::chrono::steady_clock::now();
    int acked = 0;
    for (int i = 0; i < n; ++i)
    {
        acked += client.send("WRITE=p" + std::to_string(i) + ":1.0");
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    server.join();

    CHECK(acked == n);
    // inproc round trips take microseconds; the old 500 ms sleep made this 50 s
    CHECK(secs < 2.0);
}

TEST_CASE("perf: an 800-tag publish needs a near-minimal number of datagrams")
{
    std::string msg;
    for (int i = 0; i < 800; ++i)
    {
        msg += "bus-" + std::to_string(i) + ".voltage:" + std::to_string(1.0 + i / 1000.0) + ",";
    }
    const size_t mtu = 1500;
    auto chunks = chunkMessage(msg, mtu);
    // each chunk is filled to within one tag of the MTU, so at most one extra datagram
    CHECK(chunks.size() <= msg.size() / (mtu - 64) + 1);
}
