#include "Server.hpp"

#include <functional>
#include "bennu/utility/Log.hpp"

namespace bennu {
namespace distributed {

Server::Server(const Endpoint& endpoint) :
    mSocket(zmq::socket_t(Context::the()->getContext(), ZMQ_REP)),
    mHandler(std::bind(&Server::defaultHandler, this, std::placeholders::_1))
{
    int linger = 0;
    mSocket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
    try
    {
        mSocket.bind(endpoint.str);
    }
    catch (zmq::error_t& e)
    {
        printf("E: Server bind (%s): %s\n", endpoint.str.data(), e.what());
        exit(1);
    }
}

Server::~Server()
{
    mSocket.close();
}

zmq::message_t Server::defaultHandler(const zmq::message_t& request)
{
    // TODO: add default handler
    return zmq::message_t();
}

void Server::run()
{
    int timeout = 100;
    mSocket.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    while (mRunning && mSocket.connected())
    {
        zmq::message_t request;
        try
        {
            if (!mSocket.recv(&request))
            {
                continue;
            }
        }
        catch (zmq::error_t& e)
        {
            if (e.num() == ETERM || !mRunning)
            {
                break;
            }
            printf("E: Server recv: %s", e.what());
            break;
        }
        BENNU_LOG_DEBUG("Server ---- Received request\n");

        zmq::message_t reply(mHandler(request));
        try
        {
            mSocket.send(reply);
        }
        catch (zmq::error_t& e)
        {
            printf("E: Server send: %s", e.what());
            break;
        }
        BENNU_LOG_DEBUG("Server ---- Sent reply\n");
        if (!mRunning)
        {
            break;
        }
    }
}

void Server::stop()
{
    mRunning = false;
}

} // namespace distributed
} // namespace bennu
