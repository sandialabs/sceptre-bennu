#include "Client.hpp"

#include <chrono>
#include <functional>
#include <thread>

#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

#define REQUEST_TIMEOUT     5000    //  msecs, (> 1000!)
#define REQUEST_RETRIES     3       //  Before we abandon

Client::Client(const Endpoint& endpoint) :
    mSocket(zmq::socket_t(Context::the()->getContext(), ZMQ_REQ)),
    mHandler(std::bind(&Client::defaultHandler, this, std::placeholders::_1)),
    mEndpoint(endpoint)
{
    connect();
}

Client::~Client()
{
    mSocket.close();
}

void Client::connect()
{
    mSocket = zmq::socket_t(Context::the()->getContext(), ZMQ_REQ);
    printf("I: Client connect (%s): Connecting to provider\n", mEndpoint.str.data());
    try
    {
        mSocket.connect(mEndpoint.str);
    }
    catch (zmq::error_t& e)
    {
        printf("E: Client connect (%s): %s\n", mEndpoint.str.data(), e.what());
        exit(1);
    }
    int linger = 0; // configure socket to not wait at close time
    mSocket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
}

void Client::defaultHandler(const std::string& reply)
{
    printf("I: Client received reply: %s\n", reply.data());
}

/*
 * Use ZMQ LazyPirate pattern to send reliable req/rep
 *  - http://zguide.zeromq.org/php:chapter4#Client-Side-Reliability-Lazy-Pirate-Pattern
 *  - http://zguide.zeromq.org/cpp:lpclient
 */
void Client::send(const std::string& msg)
{
    int retriesLeft = REQUEST_RETRIES;
    while (retriesLeft)
    {
        zmq::message_t message(msg+'\0'); // must include null byte
        mSocket.send(message);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bool expectReply = true;
        while (expectReply)
        {
            // Poll socket for a reply, with timeout
            zmq::pollitem_t items[] = { { mSocket, 0, ZMQ_POLLIN, 0 } };
            zmq::poll(&items[0], 1, REQUEST_TIMEOUT);
            // If we got a reply, process it
            if (items[0].revents & ZMQ_POLLIN)
            {
                zmq::message_t repMsg;
                mSocket.recv(&repMsg);
                std::string reply(repMsg.data<char>());
                std::string delim{"="}, status, data;
                try
                {
                    auto split = distributed::split(reply, delim);
                    status = split[0];
                    data = split[1];
                }
                catch (std::exception& e)
                {
                    printf("E: Client send: %s", e.what());
                }

                if (status == "ACK" || status == "ack")
                {
                    printf("I: ACK\n");
                    mHandler(data);
                }
                else if (status == "ERR" || status == "err")
                {
                    printf("I: ERR -- %s\n", data.data());
                }
                else
                {
                    printf("E: Client send: malformed reply from server -- %s\n", data.data());
                }
                expectReply = false;
                retriesLeft = 0;
                break;
            }
            else if (--retriesLeft == 0)
            {
                printf("E: Client send: server seems to be offline, abandoning\n");
                expectReply = false;
                // Old socket will be confused; close it and open a new one
                mSocket.close();
                connect();
                break;
            }
            else
            {
                printf("I: Client send: no response from server, retrying...\n");
                // Old socket will be confused; close it and open a new one
                mSocket.close();
                connect();
                // Send request again, on new socket
                mSocket.send(message);
            }
        }
    }
}

} // namespace distributed
} // namespace bennu
