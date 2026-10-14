#include "Subscriber.hpp"

#include <chrono>
#include <cstring>

#include "bennu/utility/Log.hpp"

namespace bennu {
namespace distributed {

Subscriber::Subscriber(const Endpoint& endpoint, SubscriptionHandler handler) :
    mSocket(zmq::socket_t(Context::the()->getContext(), ZMQ_DISH)),
    mHandler(std::move(handler))
{
    try
    {
        mSocket.bind(endpoint.str);
    }
    catch (zmq::error_t& e)
    {
        printf("E: Server bind (%s): %s\n", endpoint.str.data(), e.what());
        exit(1);
    }
    mSocket.join(endpoint.hash().data());
    mSubscriberThread.reset(new std::thread(std::bind(&Subscriber::run, this)));
}

Subscriber::~Subscriber()
{
    mSocket.close();
}

void Subscriber::run()
{
    auto lastLog = std::chrono::steady_clock::now();
    while (mSocket.connected())
    {
        zmq::message_t msg;
        try
        {
            mSocket.recv(&msg);
        }
        catch (zmq::error_t& e)
        {
            printf("E: Server: %s\n", e.what());
            break;
        }
        // the buffer is not guaranteed to be NUL-terminated (pybennu publisher)
        std::string data(msg.data<char>(), strnlen(msg.data<char>(), msg.size()));
        mHandler(data);
        ++mDatagrams;
        if (utility::logEnabled(utility::LOG_DEBUG))
        {
            auto now = std::chrono::steady_clock::now();
            if (now - lastLog >= std::chrono::seconds(10))
            {
                lastLog = now;
                BENNU_LOG_DEBUG("D: Subscriber: datagrams_received=%llu\n", (unsigned long long)mDatagrams);
            }
        }
    }
}

} // namespace distributed
} // namespace bennu
