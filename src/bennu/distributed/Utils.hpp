#ifndef BENNU_DISTRIBUTED_UTILS_HPP
#define BENNU_DISTRIBUTED_UTILS_HPP

#include <cstdlib>
#include <functional> // std::hash
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "zmq/zmq.hpp"

#include "bennu/utility/Singleton.hpp"

namespace bennu {
namespace distributed {

class Context : public utility::Singleton<Context>
{
public:
    friend class utility::Singleton<Context>;

    ~Context() = default;

    zmq::context_t& getContext()
    {
        return mContext;
    }

private:
    Context() :
        mContext(1)
    {}

    Context(const Context&) :
        mContext(1)
    {}

    zmq::context_t mContext;
};

struct Endpoint
{
    std::string str;

    /*
     *  ZMQ RADIO/DISH Group has a max character limit of 15.
     *
     *  std::hash is 64 bits (16 hex characters), so convert
     *  to hex string and truncate the last character.
     */
    std::string hash() const
    {
        //  If we have a semicolon then we should have an interface specifier in the URL
        std::string delim{";"};
        std::string str2 = this->str;
        size_t pos = str2.find(delim);
        if (pos != std::string::npos)
        {
            delim = "/";
            size_t start = str2.find(delim) + 2;
            str2.erase(start, pos - (start - 1));
        }
        auto hash = std::hash<std::string>{}(str2);
        std::stringstream ss;
        ss << std::hex << hash;
        return ss.str().substr(0, ss.str().length() - 1);
    }
};

/*
 * Split a string using delimiter. Returns vector of strings.
 */
inline std::vector<std::string> split(const std::string& phrase, const std::string& delimiter)
{
    std::vector<std::string> list;
    size_t start = 0, pos;
    while ((pos = phrase.find(delimiter, start)) != std::string::npos)
    {
        list.push_back(phrase.substr(start, pos - start));
        start = pos + delimiter.length();
    }
    list.push_back(phrase.substr(start));
    return list;
}

/*
 * Split a comma-delimited message into chunks of "part," pieces, each
 * shorter than mtu where possible. Empty parts are skipped; a single part
 * longer than mtu is returned alone.
 */
inline std::vector<std::string> chunkMessage(const std::string& msg, size_t mtu)
{
    std::vector<std::string> chunks;
    std::string chunk;
    for (const auto& p : split(msg, ","))
    {
        if (p.empty())
        {
            continue;
        }
        std::string part = p + ",";
        if (!chunk.empty() && chunk.length() + part.length() >= mtu)
        {
            chunks.push_back(chunk);
            chunk.clear();
        }
        chunk += part;
    }
    if (!chunk.empty())
    {
        chunks.push_back(chunk);
    }
    return chunks;
}

/*
 * libzmq sends a RADIO message over UDP as [group length (1 B)][group][body], at most
 * 8192 B, inside 28 B of IPv4 and UDP headers. A datagram larger than the link MTU is
 * IP-fragmented, and losing either fragment loses the whole datagram.
 */
constexpr size_t IP_UDP_HEADERS = 28;
constexpr size_t MAX_UDP_MSG = 8192;

// Link MTU for publish datagrams from env BENNU_PUBLISH_MTU (default 1500).
// Throws if it is not an integer from 576 to 8220.
inline size_t publishLinkMtu()
{
    const char* env = std::getenv("BENNU_PUBLISH_MTU");
    if (!env || !*env)
    {
        return 1500;
    }
    std::string s(env);
    // stoul would also accept leading whitespace and '+'; Python accepts ASCII digits only
    bool digits = s.find_first_not_of("0123456789") == std::string::npos;
    size_t used = 0;
    unsigned long mtu = 0;
    try
    {
        mtu = digits ? std::stoul(s, &used) : 0;
    }
    catch (const std::exception&)
    {
        used = 0;
    }
    if (!digits || used != s.size() || mtu < 576 || mtu > MAX_UDP_MSG + IP_UDP_HEADERS)
    {
        throw std::invalid_argument("BENNU_PUBLISH_MTU must be an integer link MTU from 576 to 8220, got '" + s + "'");
    }
    return mtu;
}

// Largest datagram body (including the trailing NUL) that fits linkMtu for this group.
inline size_t publishBodyLimit(size_t linkMtu, size_t groupLength)
{
    return linkMtu - IP_UDP_HEADERS - 1 - groupLength;
}

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_UTILS_HPP
