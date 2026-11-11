#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

std::string exec(const char* cmd)
{
    std::array<char, 512> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    return result;
}

// Stop every process whose command line contains `name` and wait until they have all exited.
// bennu-field-device shuts down gracefully on SIGTERM (~200 ms), so a bare pkill lets the next
// test start while the old daemon still holds its ports (e.g. DNP3 :20000) and the new one
// fails to bind. Escalates to SIGKILL after 10 s so a hung shutdown cannot stall the suite.
// The last character is wrapped in a bracket expression (abc -> ab[c]) so the pattern never matches
// this shell itself.
void stopProcesses(const std::string& name)
{
    const std::string pat = "'" + name.substr(0, name.size() - 1) + "[" + name.back() + "]'";
    const std::string cmd = "pkill -f " + pat + "; for i in $(seq 1 200); do pgrep -f " + pat +
                            " >/dev/null || exit 0; sleep 0.05; done; pkill -9 -f " + pat + "; true";
    exec(cmd.c_str());
}
