#ifndef BENNU_DISTRIBUTED_PUBLISHER_HPP
#define BENNU_DISTRIBUTED_PUBLISHER_HPP

#include <string>

#include "zmq/zmq.hpp"

#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

class Publisher
{
public:
    Publisher(const Endpoint& endpoint);

    ~Publisher();

    void publish(std::string& msg);

    void publish(zmq::message_t& msg);

private:
    zmq::socket_t mSocket;
    std::string mGroup;
    uint mMTU;

};

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_PUBLISHER_HPP
