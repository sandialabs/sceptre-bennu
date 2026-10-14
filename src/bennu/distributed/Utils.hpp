#ifndef BENNU_DISTRIBUTED_UTILS_HPP
#define BENNU_DISTRIBUTED_UTILS_HPP

#include <functional> // std::hash
#include <sstream>
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

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_UTILS_HPP
