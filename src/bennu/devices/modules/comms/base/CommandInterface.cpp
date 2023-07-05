#include "CommandInterface.hpp"

#include <sstream>
#include <string>

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/base/CommsClient.hpp"

namespace bennu {
namespace comms {

using bennu::comms::RegisterDescriptor;

CommandInterface::CommandInterface(const distributed::Endpoint& endpoint, std::shared_ptr<CommsClient> client) :
    bennu::distributed::Server(endpoint),
    mClient(client)
{
    setHandler(std::bind(&CommandInterface::messageHandler, this, std::placeholders::_1));
}

zmq::message_t CommandInterface::messageHandler(const zmq::message_t& req)
{
    std::string request(req.data<char>());
    std::string opDelim{"="}, op, tag;
    std::string reply;
    try {
        auto split = distributed::split(request, opDelim);
        op = split[0];
        tag = split[1];
        printf("Received %s request for tag %s\n", op.data(), tag.data());
    } catch (std::exception& e) {
        printf("Err: %s", e.what());
    }

    if (op == "QUERY" || op == "query")
    {
        auto tags = mClient.lock()->getTags();
        if (tags.size() == 0)
        {
            reply += "ERR=Client does not have any tag mappings";
        }
        else
        {
            reply += "ACK=";
            for (auto iter = tags.begin(); iter != tags.end(); ++iter)
            {
                reply += *iter;
                reply += ",";
            }
        }
    }
    else if (op == "READ" || op == "read")
    {
        if (!mClient.lock()->isValidTag(tag))
        {
            reply += "ERR=Client does not have a mapping for tag '" + tag + "'";
        }
        else
        {
            RegisterDescriptor rd;
            auto result = mClient.lock()->readTag(tag, rd);
            if (result.status)
            {
                switch (rd.mRegisterType)
                {
                    case comms::eValueReadWrite:
                    case comms::eValueReadOnly:
                    {
                        reply += "ACK=" + tag + ":" + std::to_string(rd.mFloatValue);
                        break;
                    }
                    case comms::eStatusReadWrite:
                    case comms::eStatusReadOnly:
                    {
                        std::string str = rd.mStatus ? "true" : "false";
                        reply += "ACK=" + tag + ":" + str;
                        break;
                    }
                    default:
                    {
                        reply += "ERR=Client had a problem reading tag '" + tag + "'";
                        break;
                    }
                }
            }
            else
            {
                std::string msg(result.message); // convert char* to string
                reply += "ERR=Failed reading tag '" + tag + "': " + msg;
            }
        }
    }
    else if (op == "WRITE" || op == "write")
    {
        std::string valDelim{":"}, val{""};
        StatusMessage result;
        try {
            auto split = distributed::split(tag, valDelim);
            tag = split[0];
            val = split[1];
        } catch (std::exception& e) {
            printf("Err: %s", e.what());
        }

        if (!mClient.lock()->isValidTag(tag))
        {
            reply += "ERR=Client does not have a mapping for tag '" + tag + "'";
        }
        else
        {
            if (val == "true" || val == "false")
            {
                bool value = val == "true" ? true : false;
                result = mClient.lock()->writeBinaryTag(tag, value);
            }
            else
            {
                result = mClient.lock()->writeAnalogTag(tag, std::stod(val));
            }

            if (result.status)
            {
                reply += "ACK=Wrote tag " + tag + " -- " + val;
            }
            else
            {
                std::string msg(result.message); // convert char* to string
                reply += "ERR=Failed writing tag '" + tag + "': " + msg;
            }
        }
    }
    else
    {
        reply += "ERR=Unknown command type (must be QUERY|READ|WRITE)";
    }
    printf("Sending reply for tag %s -- %s\n", tag.data(), reply.data());
    zmq::message_t repMsg(reply+'\0'); // must include null byte
    return repMsg;
}

} // namespace comms
} // namespace bennu
