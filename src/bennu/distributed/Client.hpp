#ifndef BENNU_DISTRIBUTED_CLIENT_HPP
#define BENNU_DISTRIBUTED_CLIENT_HPP

#include <string>

#include "zmq/zmq.hpp"

#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

class Client
{
public:
    typedef std::function<void (const std::string& reply)> ReplyHandler;

    Client(const Endpoint& endpoint);

    ~Client();

    void send(const std::string& msg);

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

};

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_CLIENT_HPP
