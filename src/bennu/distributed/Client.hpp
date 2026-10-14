#ifndef BENNU_DISTRIBUTED_CLIENT_HPP
#define BENNU_DISTRIBUTED_CLIENT_HPP

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "zmq/zmq.hpp"

#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

class Client
{
public:
    typedef std::function<void(const std::string& reply)> ReplyHandler;

    Client(const Endpoint& endpoint);

    ~Client();

    // true iff an ACK reply was received
    bool send(const std::string& msg);

    // true if the last send() got any reply (ACK or ERR); false means the provider never answered
    bool replied() const
    {
        return mReplied;
    }

    // "p1:v1,p2:v2" with the same value formatting as writePoint; binaries first, then analogs
    static std::string buildWritePayload(const std::vector<std::pair<std::string, bool>>& bins,
                                         const std::vector<std::pair<std::string, double>>& anals)
    {
        std::string out;
        for (auto& b : bins)
        {
            out += (out.empty() ? "" : ",") + b.first + ":" + (b.second ? "true" : "false");
        }
        for (auto& a : anals)
        {
            out += (out.empty() ? "" : ",") + a.first + ":" + std::to_string(a.second);
        }
        return out;
    }

    bool writePoints(const std::string& payload)
    {
        return send("WRITE=" + payload);
    }

    void setHandler(ReplyHandler handler)
    {
        mHandler = handler;
    }

    void defaultHandler(const std::string& reply);

    void writePoint(const std::string& tag, const double value)
    {
        std::string msg{"WRITE=" + tag + ":" + std::to_string(value)};
        send(msg);
    }

    void writePoint(const std::string& tag, const bool value)
    {
        auto val = value ? "true" : "false";
        std::string msg{"WRITE=" + tag + ":" + val};
        send(msg);
    }

private:
    void connect();

    zmq::socket_t mSocket;
    ReplyHandler mHandler;
    Endpoint mEndpoint;
    bool mReplied = false;
};

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_CLIENT_HPP
