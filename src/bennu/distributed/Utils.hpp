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
            str2.erase(start, pos - (start-1));
        }
        auto hash = std::hash<std::string>{}(str2);
        std::stringstream ss;
        ss << std::hex << hash;
        return ss.str().substr(0, ss.str().length()-1);
    }
};

/*
 * Split a string using delimiter. Returns vector of strings.
 */
inline std::vector<std::string> split(std::string& phrase, std::string& delimiter)
{
    std::vector<std::string> list;
    size_t pos = 0;
    std::string token;
    while ((pos = phrase.find(delimiter)) != std::string::npos)
    {
        token = phrase.substr(0, pos);
        list.push_back(token);
        phrase.erase(0, pos + delimiter.length());
    }
    list.push_back(phrase);
    return list;
}

} // namespace distributed
} // namespace bennu

#endif // BENNU_DISTRIBUTED_UTILS_HPP
