#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing server")
{
    exec("bennu-field-device --f ../data/configs/ep/dnp3-server.xml >fd-server.out 2>&1 &");
    exec("sleep .1");
    exec("pkill -f bennu-field-device");
    // Control points are registered as output status (groups 10/40) rather than
    // as inputs, so inputs and outputs are counted separately.
    std::string res("Listening on: 127.0.0.1:20000\nBinary Size is 1 and Analog Size is 1.\nBinary Output Size is 1 and Analog Output Size is 1.\n");
    CHECK(exec("grep -o -e 'Binary Size is 1 and Analog Size is 1.' -e 'Binary Output Size is 1 and Analog Output Size is 1.' -e 'Listening on: 127.0.0.1:20000' fd-server.out") == res);
}
