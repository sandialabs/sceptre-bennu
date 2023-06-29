#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing server")
{
    exec("bennu-field-device --f ../data/configs/ep/dnp3-server.xml >fd-server.out 2>&1 &");
    exec("sleep .1");
    exec("pkill -f bennu-field-device");
    std::string res("Listening on: 127.0.0.1:20000\nBinary Size is 2 and Analog Size is 2.\n");
    CHECK(exec("grep -o -e 'Binary Size is 2 and Analog Size is 2.' -e 'Listening on: 127.0.0.1:20000' fd-server.out") == res);
}
