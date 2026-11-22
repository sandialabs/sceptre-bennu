#include "doctest.h"
#include <string>

extern std::string exec(const char*);
extern void stopProcesses(const std::string& name);

TEST_CASE("testing bennu-test-ep-server")
{
    stopProcesses("bennu-test-ep-server");
    exec("bennu-test-ep-server --d 1 >ep.out 2>&1 &");
    exec("sleep .1");
    stopProcesses("bennu-test-ep-server");
    std::string res("Server running on tcp://127.0.0.1:5555\nPublisher running on udp://239.0.0.1:40000\n");
    CHECK(exec("cat ep.out") == res);
}

TEST_CASE("bennu-test-ep-server QUERY reply has no empty trailing tag")
{
    stopProcesses("bennu-test-ep-server");
    exec("bennu-test-ep-server >ep.out 2>&1 &");
    exec("sleep 1");
    exec("bennu-probe --endpoint tcp://127.0.0.1:5555 --command query >ep-query.out 2>&1");
    stopProcesses("bennu-test-ep-server");
    CHECK(exec("grep -c -E '^[[:space:]]+bus-1.active$' ep-query.out") == "1\n");
    // the probe prints one line per comma-separated item, so a trailing comma shows as a blank line
    CHECK(exec("grep -c -E '^[[:space:]]*$' ep-query.out") == "0\n");
}
