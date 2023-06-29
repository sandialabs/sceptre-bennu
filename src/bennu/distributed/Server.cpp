#include "Server.hpp"

#include <functional>

namespace bennu {
namespace distributed {

Server::Server(const Endpoint& endpoint) :
    mSocket(zmq::socket_t(Context::the()->getContext(), ZMQ_REP)),
    mHandler(std::bind(&Server::defaultHandler, this, std::placeholders::_1))
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
}

Server::~Server()
{
    mSocket.close();
}

zmq::message_t Server::defaultHandler(const zmq::message_t& request)
{
    // TODO: add default handler
}

void Server::run()
{
    while (mSocket.connected())
    {
        zmq::message_t request;
        try
        {
            mSocket.recv(&request);
        }
        catch (zmq::error_t& e)
        {
            printf("E: Server recv: %s", e.what());
            break;
        }
        printf("Server ---- Received request\n");

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
        printf("Server ---- Sent reply\n");
    }
}

} // namespace distributed
} // namespace bennu
