#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing client")
{
    exec("bennu-field-device --f ../data/configs/ep/dnp3-client.xml >fd-client.out 2>&1 &");
    exec("sleep .1");
    exec("pkill -f bennu-field-device");
    std::string res("Initialized DNP3-CLIENT -- Address: 1, RTU Connection: tcp://127.0.0.1:20000\n");
    CHECK(exec("grep 'DNP3-CLIENT' fd-client.out") == res);
}
