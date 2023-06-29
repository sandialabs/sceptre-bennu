#ifndef BENNU_DISTRIBUTED_SUBSCRIBER_HPP
#define BENNU_DISTRIBUTED_SUBSCRIBER_HPP

#include <functional> // std::function
#include <memory>
#include <string>
#include <thread>

#include "zmq/zmq.hpp"

#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

class Subscriber
{
public:
    typedef std::function<void (std::string& data)> SubscriptionHandler;

    Subscriber(const Endpoint& endpoint);

    ~Subscriber();

    void setHandler(SubscriptionHandler handler)
    {
        mHandler = handler;
    }

    void run();

private:
    void defaultHandler(std::string& data);
    zmq::socket_t mSocket;
    const std::string mGroup;
    SubscriptionHandler mHandler;
    std::shared_ptr<std::thread> mSubscriberThread;
};

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_SUBSCRIBER_HPP
