#include "DirectLoggable.hpp"

#include <iostream>

namespace bennu {
namespace utility {

utility::DirectLoggable::DirectLoggable(const std::string& name) :
    utility::Loggable(name)
{
}

utility::DirectLoggable::~DirectLoggable()
{
    if (mEventStream.is_open())
    {
        mEventStream.close();
    }

    if (mDebugStream.is_open())
    {
        mDebugStream.close();
    }
}

void utility::DirectLoggable::configureEventLogging(const std::string& stream)
{
    mEventStream.open(stream.c_str());
    if (!mEventStream.is_open())
    {
        std::cerr << "ERROR: There was a problem opening the event logging file: " << stream << std::endl;
    }
}

void utility::DirectLoggable::configureDebugLogging(const std::string& stream)
{
    mDebugStream.open(stream.c_str());
    if (!mDebugStream.is_open())
    {
        std::cerr << "ERROR: There was a problem opening the debug logging file: " << stream << std::endl;
    }
}

void utility::DirectLoggable::logEvent(const std::string& event_name, const std::string& level, const std::string& message)
{
    if (!mEventStream.is_open())
    {
        return;
    }

    boost::posix_time::ptime timestamp = boost::posix_time::second_clock::local_time();

    std::ostringstream os;
    os << timestamp.date() << "-" << timestamp.time_of_day();

    mEventLock.lock();
    {

        mEventStream << mLogEventSequence << "," << os.str() << "," << level << "," << mName << "," << event_name << "," << message << std::endl;

        mLogEventSequence++;

        mEventLock.unlock();
    }
}

void utility::DirectLoggable::logDebug(const std::string& level, const std::string& message)
{
    if (!mDebugStream.is_open())
    {
        return;
    }

    boost::posix_time::ptime timestamp = boost::posix_time::second_clock::local_time();

    std::ostringstream os;
    os << timestamp.date() << "-" << timestamp.time_of_day();

    if (!mDebugLock.try_lock_for(std::chrono::milliseconds(100)))
    {

        mDebugStream << mDebugLogEventSequence << "," << os.str() << "," << level << "," << mName << "," << "debug" << "," << message << std::endl;

        mDebugLogEventSequence++;

        mDebugLock.unlock();
    }
}

} // namespace utility
} // namespace bennu
