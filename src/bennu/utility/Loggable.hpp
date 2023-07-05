#ifndef BENNU_UTILITY_LOGGABLE_HPP
#define BENNU_UTILITY_LOGGABLE_HPP

#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace bennu {
namespace utility {

class Loggable
{
public:
    Loggable(const std::string& name) :
        mName(name),
        mFilterInfo(""),
        mTimestamp(),
        mLogEventSequence(0),
        mDebugLogEventSequence(0)
    {
    }

    virtual void configureEventLogging(const std::string& log) = 0;

    virtual void configureDebugLogging(const std::string& log) = 0;

    void setAdditionalFilterInformation(const std::string& info)
    {
        mFilterInfo = info;
    }

    virtual void logEvent(const std::string& event_name, const std::string& level, const std::string& message) = 0;

    virtual void logDebug(const std::string& level, const std::string& message) = 0;

    void setName(const std::string& name)
    {
        mName = name;
    }

protected:
    std::string mName;

    std::string mFilterInfo;

    // The timestamp that is set each time a message is processed.
    std::ostringstream mTimestamp;

    size_t mLogEventSequence;

    size_t mDebugLogEventSequence;

    std::timed_mutex mEventLock;

    std::timed_mutex mDebugLock;

    Loggable(const Loggable&);

    Loggable& operator =(const Loggable&);

};

} // namespace utility
} // namespace bennu

#endif // BENNU_UTILITY_LOGGABLE_HPP
