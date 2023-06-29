#ifndef BENNU_UTILITY_DIRECTLOGGABLE_HPP
#define BENNU_UTILITY_DIRECTLOGGABLE_HPP

#include <fstream>
#include <string>

#include "bennu/utility/Loggable.hpp"

namespace bennu {
namespace utility {

class DirectLoggable : public Loggable
{
public:
    DirectLoggable(const std::string& name);

    virtual ~DirectLoggable();

    virtual void configureEventLogging(const std::string& stream);

    virtual void configureDebugLogging(const std::string& stream);

    virtual void logEvent(const std::string& event_name, const std::string& level, const std::string& message);

    virtual void logDebug(const std::string& level, const std::string& message);

private:
    std::ofstream mEventStream;
    std::ofstream mDebugStream;
    DirectLoggable(const DirectLoggable&);
    DirectLoggable& operator =(const DirectLoggable&);

};

} // namespace utility
} // namespace bennu

#endif // BENNU_UTILITY_DIRECTLOGGABLE_HPP
