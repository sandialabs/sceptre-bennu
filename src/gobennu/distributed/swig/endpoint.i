%module endpoint

%include "std_string.i"

%{
    #define SWIG_FILE_WITH_INIT
    #include <functional>
    #include <sstream>
    #include <string>
    struct Endpoint
    {
        std::string str;
        std::string hash() const
        {
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
%}

#include <functional>
#include <sstream>
#include <string>
struct Endpoint
{
    std::string str;
    std::string hash() const
    {
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
