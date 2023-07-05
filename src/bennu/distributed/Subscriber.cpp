#include "Subscriber.hpp"

namespace bennu {
namespace distributed {

Subscriber::Subscriber(const Endpoint& endpoint) :
    mSocket(zmq::socket_t(Context::the()->getContext(), ZMQ_DISH)),
    mHandler(std::bind(&Subscriber::defaultHandler, this, std::placeholders::_1))
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

void Subscriber::defaultHandler(std::string& data)
{
    // TODO: add default handler
    return;
}

void Subscriber::run()
{
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
        std::string data(msg.data<char>());
        mHandler(data);
    }
}

} // namespace distributed
} // namespace bennu
