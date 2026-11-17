#include "Provider.hpp"

#include <functional>
#include "bennu/utility/Log.hpp"

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
    // senders may or may not include a trailing NUL, so read exactly request.size() bytes
    std::string req(request.data<char>(), request.size());
    while (!req.empty() && req.back() == '\0')
    {
        req.pop_back();
    }
    std::string opDelim{"="}, op, payload;
    std::string reply;
    try
    {
        auto split = distributed::split(req, opDelim);
        op = split[0];
        payload = split.at(1);
        BENNU_LOG_DEBUG("Received %s request for payload %s\n", op.data(), payload.data());
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
        std::unordered_map<std::string, std::string> tags;
        bool malformed = false;

        std::string pointDelim{","};
        for (auto& point : distributed::split(payload, pointDelim))
        {
            if (point.empty())
            {
                continue; // trailing comma
            }
            std::string valDelim{":"};
            auto kv = distributed::split(point, valDelim);
            if (kv.size() < 2)
            {
                printf("Err: malformed WRITE point '%s'\n", point.data());
                malformed = true;
                break;
            }
            tags[kv[0]] = kv[1]; // last value wins
        }

        if (malformed || tags.empty())
        {
            reply += "ERR=Malformed WRITE payload";
        }
        else
        {
            reply += write(tags);
        }
    }
    else
    {
        reply += "ERR=Unknown command type '" + op + "'";
    }
    BENNU_LOG_DEBUG("Sending reply for payload %s -- %s\n", payload.data(), reply.data());
    zmq::message_t repMsg(reply + '\0'); // must include null byte
    return repMsg;
}

} // namespace distributed
} // namespace bennu
