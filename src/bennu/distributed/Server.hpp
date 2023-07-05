#ifndef BENNU_DISTRIBUTED_SERVER_HPP
#define BENNU_DISTRIBUTED_SERVER_HPP

#include <functional> // std::function
#include <string>

#include "zmq/zmq.hpp"

#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

class Server
{
public:
    typedef std::function<zmq::message_t (const zmq::message_t& request)> RequestHandler;

    Server(const Endpoint& endpoint);

    ~Server();

    void setHandler(RequestHandler handler)
    {
        mHandler = handler;
    }

    zmq::message_t defaultHandler(const zmq::message_t& request);

    void run();

private:
    zmq::socket_t mSocket;
    RequestHandler mHandler;

};

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_SERVER_HPP
