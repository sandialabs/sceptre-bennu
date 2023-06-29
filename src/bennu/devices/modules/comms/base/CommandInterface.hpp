#ifndef BENNU_FIELDDEVICE_COMMS_COMMANDINTERFACE_HPP
#define BENNU_FIELDDEVICE_COMMS_COMMANDINTERFACE_HPP

#include <functional>
#include <memory>
#include <thread>

#include "zmq/zmq.hpp"

#include "bennu/distributed/Server.hpp"
#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace comms {

class CommsClient;

class CommandInterface : public bennu::distributed::Server
{
public:
    CommandInterface(const distributed::Endpoint& endpoint, std::shared_ptr<CommsClient> client);

    void start()
    {
        mThread.reset(new std::thread(std::bind(&CommandInterface::run, this)));
    }

protected:
    virtual zmq::message_t messageHandler(const zmq::message_t& req);

private:
    std::weak_ptr<CommsClient> mClient;
    std::shared_ptr<std::thread> mThread;

};

} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_COMMANDINTERFACE_HPP
