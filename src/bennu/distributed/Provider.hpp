#ifndef BENNU_DISTRIBUTED_PROVIDER_HPP
#define BENNU_DISTRIBUTED_PROVIDER_HPP

#include <memory>
#include <string>
#include <thread>

#include "zmq/zmq.hpp"

#include "bennu/distributed/Publisher.hpp"
#include "bennu/distributed/Server.hpp"
#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

class Provider
{
public:
    Provider(const Endpoint& serverEndpoint, const Endpoint& publishEndpoint);

    virtual ~Provider() = default;

    void run();

    void publish(std::string& msg)
    {
        mPublisher->publish(msg);
    }

    void publish(zmq::message_t& msg)
    {
        mPublisher->publish(msg);
    }

protected:
    /* Methods to process incoming message. Inheriting providers must implement these */
    // Must return "ACK=tag1,tag2,..." or "ERR=<error message>"
    virtual std::string query() = 0;
    // Must return "ACK=<value>" or "ERR=<error message>"
    virtual std::string read(const std::string& tag) = 0;
    // Must return "ACK=<success message>" or "ERR=<error message>"
    virtual std::string write(const std::unordered_map<std::string,std::string>& tags) = 0;
    /* Method to periodically publish data. Inheriting providers must implement */
    [[ noreturn ]] virtual void periodicPublish() = 0;

private:
    zmq::message_t messageHandler(const zmq::message_t& request);

    std::shared_ptr<Server> mServer;
    std::shared_ptr<Publisher> mPublisher;
    std::shared_ptr<std::thread> mPublishThread;

};

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_PROVIDER_HPP
