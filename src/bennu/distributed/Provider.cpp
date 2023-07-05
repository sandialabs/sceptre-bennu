#include "Provider.hpp"

#include <functional>

#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

Provider::Provider(const Endpoint& serverEndpoint, const Endpoint& publishEndpoint)
{
    mPublisher.reset(new Publisher(publishEndpoint));
    mServer.reset(new Server(serverEndpoint));
    mServer->setHandler(std::bind(&Provider::messageHandler, this, std::placeholders::_1));
    printf("Server running on %s\n", serverEndpoint.str.data());
    printf("Publisher running on %s\n", publishEndpoint.str.data());
    fflush(stdout);
}

void Provider::run()
{
    mPublishThread.reset(new std::thread(std::bind(&Provider::periodicPublish, this)));
    mServer->run();
}

// message requests should be in the form:
// "QUERY="
// "READ=<tag name>"
// "WRITE=<tag name>:<value>[,<tag name>:<value>...]"
zmq::message_t Provider::messageHandler(const zmq::message_t& request)
{
    std::string req(request.data<char>());
    std::string opDelim{"="}, op, payload;
    std::string reply;
    try
    {
        auto split = distributed::split(req, opDelim);
        op = split[0];
        payload = split[1];
        printf("Received %s request for payload %s\n", op.data(), payload.data());
    }
    catch (std::exception& e)
    {
        printf("Err: %s", e.what());
    }

    if (op == "QUERY" || op == "query")
    {
        reply += query();
    }
    else if (op == "READ" || op == "read")
    {
        reply += read(payload);
    }
    else if (op == "WRITE" || op == "write")
    {
        std::unordered_map<std::string,std::string> tags;

        std::string pointDelim{","};
        try
        {
            auto split = distributed::split(payload, pointDelim);

            for (auto &point : split)
            {
                std::string valDelim{":"};
                try
                {
                    auto split = distributed::split(point, valDelim);
                    tags.insert({split[0], split[1]});
                }
                catch (std::exception& e)
                {
                    printf("Err: %s", e.what());
                }
            }
        }
        catch(const std::exception& e)
        {
            printf("Err: %s", e.what());
        }

        reply += write(tags);
    }
    else
    {
        reply += "ERR=Unknown command type '" + op + "'";
    }
    printf("Sending reply for payload %s -- %s\n", payload.data(), reply.data());
    zmq::message_t repMsg(reply+'\0'); // must include null byte
    return repMsg;
}

} // namespace distributed
} // namespace bennu
