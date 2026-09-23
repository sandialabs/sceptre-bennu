#include "doctest.h"
#include <string>

extern std::string exec(const char*);

// Launches an IEC 60870-5-104 outstation configured with select-before-operate
// enabled and confirms it starts up in SBO mode. Mirrors the style of
// test_fd_server.cpp (launch the binary, sleep, grep the captured output).
TEST_CASE("testing iec60870-5-104 select-before-operate server startup")
{
    exec("bennu-field-device --f ../data/configs/ep/iec60870-5-104-server-sbo.xml >fd-104-sbo.out 2>&1 &");
    exec("sleep .1");
    exec("pkill -f bennu-field-device");
    std::string res("Initialized IEC60870-5-104 server: tcp://127.0.0.1:2404 (select-before-operate enabled)\n");
    CHECK(exec("grep -o -e 'Initialized IEC60870-5-104 server: tcp://127.0.0.1:2404 (select-before-operate enabled)' fd-104-sbo.out") == res);
}
