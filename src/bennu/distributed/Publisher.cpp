#include "Publisher.hpp"

#include "bennu/distributed/Utils.hpp"
#include "bennu/utility/Log.hpp"

namespace bennu {
namespace distributed {

Publisher::Publisher(const Endpoint& endpoint) :
    mSocket(zmq::socket_t(Context::the()->getContext(), ZMQ_RADIO)),
    mGroup(endpoint.hash()),
    mMaxBody(publishBodyLimit(publishLinkMtu(), mGroup.size()))
{
    mSocket.connect(endpoint.str);
}

Publisher::~Publisher()
{
    mSocket.close();
}

void Publisher::publish(std::string& msg)
{
    // the body carries a NUL, so it holds at most mMaxBody - 1 message bytes
    if (msg.length() >= mMaxBody)
    {
        const auto chunks = chunkMessage(msg, mMaxBody);
        BENNU_LOG_DEBUG("D: Publisher: %zu bytes -> %zu datagrams\n", msg.length(), chunks.size());
        for (const auto& chunk : chunks)
        {
            zmq::message_t partMsg(chunk + '\0'); // must include null byte
            publish(partMsg);
        }
    }
    else
    {
        zmq::message_t message(msg + '\0'); // must include null byte
        publish(message);
    }
}

void Publisher::publish(zmq::message_t& msg)
{
    msg.set_group(mGroup.data());
    mSocket.send(msg);
}

} // namespace distributed
} // namespace bennu
