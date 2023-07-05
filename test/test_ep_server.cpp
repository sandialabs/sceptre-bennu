#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing bennu-test-ep-server")
{
    exec("bennu-test-ep-server --d 1 >ep.out 2>&1 &");
    exec("pkill -f bennu-test-ep-server");
    std::string res("Server running on tcp://127.0.0.1:5555\nPublisher running on udp://239.0.0.1:40000\n");
    CHECK(exec("cat ep.out") == res);
}
