#ifndef BENNU_UTILITY_LOG_HPP
#define BENNU_UTILITY_LOG_HPP

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace bennu {
namespace utility {

enum LogLevel
{
    LOG_ERROR = 0,
    LOG_WARN = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3
};

// "error|warn|info|debug" or "0-3" -> LogLevel, or -1 if unrecognised.
inline int parseLogLevel(std::string s)
{
    for (auto& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (s == "error" || s == "0")
    {
        return LOG_ERROR;
    }
    if (s == "warn" || s == "warning" || s == "1")
    {
        return LOG_WARN;
    }
    if (s == "info" || s == "2")
    {
        return LOG_INFO;
    }
    if (s == "debug" || s == "3")
    {
        return LOG_DEBUG;
    }
    return -1;
}

// Process-wide level. Starts from env BENNU_LOG_LEVEL (unset or invalid -> INFO) and can
// be changed at runtime (see LogControl.hpp). Env is process-global so every shared lib agrees.
inline std::atomic<int>& logLevelRef()
{
    static std::atomic<int> level([]() {
        const char* env = std::getenv("BENNU_LOG_LEVEL");
        int l = env ? parseLogLevel(env) : -1;
        return l < 0 ? static_cast<int>(LOG_INFO) : l;
    }());
    return level;
}

inline int logLevel()
{
    return logLevelRef().load(std::memory_order_relaxed);
}

inline void setLogLevel(int level)
{
    logLevelRef().store(level, std::memory_order_relaxed);
}

inline bool logEnabled(LogLevel l)
{
    return logLevel() >= l;
}

} // namespace utility
} // namespace bennu

// Level is checked before formatting. ERROR/WARN -> stderr, INFO/DEBUG -> stdout.
#define BENNU_LOG_(lvl, stream, ...)           \
    do                                         \
    {                                          \
        if (::bennu::utility::logEnabled(lvl)) \
        {                                      \
            std::fprintf(stream, __VA_ARGS__); \
        }                                      \
    } while (0)

#define BENNU_LOG_ERROR(...) BENNU_LOG_(::bennu::utility::LOG_ERROR, stderr, __VA_ARGS__)
#define BENNU_LOG_WARN(...) BENNU_LOG_(::bennu::utility::LOG_WARN, stderr, __VA_ARGS__)
#define BENNU_LOG_INFO(...) BENNU_LOG_(::bennu::utility::LOG_INFO, stdout, __VA_ARGS__)
#define BENNU_LOG_DEBUG(...) BENNU_LOG_(::bennu::utility::LOG_DEBUG, stdout, __VA_ARGS__)

#endif
