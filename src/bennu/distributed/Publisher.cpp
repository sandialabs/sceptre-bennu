#include "Publisher.hpp"

#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace distributed {

Publisher::Publisher(const Endpoint& endpoint) :
    mSocket(zmq::socket_t(Context::the()->getContext(), ZMQ_RADIO)),
    mGroup(endpoint.hash()),
    mMTU(1500)
{
    mSocket.connect(endpoint.str);
}

Publisher::~Publisher()
{
    mSocket.close();
}

void Publisher::publish(std::string& msg)
{
    uint size = msg.length();
    if (size > mMTU)
    {
	std::string delim{","}, temp, message;
        auto parts = distributed::split(msg, delim);
	uint count = 0;
        for (const auto& p : parts)
        {
	    std::string part{p + ","};
	    count += part.length();
	    temp = message + part;
	    // If adding the part is smaller the MTU and we haven't consumed all the msg...
	    if (temp.length() < mMTU && count <= size)
	    {
                message = temp;
            }
	    // Otherwise, send the message without 'part' and reset message
            else
            {
		zmq::message_t partMsg(message+'\0'); // must include null byte
		publish(partMsg);
            	message = part;
	    }
        }
    }
    else
    {
        zmq::message_t message(msg+'\0'); // must include null byte
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
