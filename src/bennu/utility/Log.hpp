#ifndef BENNU_UTILITY_LOG_HPP
#define BENNU_UTILITY_LOG_HPP

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

// Level comes from env BENNU_LOG_LEVEL (error|warn|info|debug or 0-3), read once.
// Unset or invalid -> INFO. Env is process-global so every shared lib agrees.
inline int logLevel()
{
    static const int level = []() {
        const char* env = std::getenv("BENNU_LOG_LEVEL");
        if (!env)
        {
            return static_cast<int>(LOG_INFO);
        }
        std::string s(env);
        for (auto& c : s)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (s == "error" || s == "0")
        {
            return static_cast<int>(LOG_ERROR);
        }
        if (s == "warn" || s == "warning" || s == "1")
        {
            return static_cast<int>(LOG_WARN);
        }
        if (s == "debug" || s == "3")
        {
            return static_cast<int>(LOG_DEBUG);
        }
        return static_cast<int>(LOG_INFO);
    }();
    return level;
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
