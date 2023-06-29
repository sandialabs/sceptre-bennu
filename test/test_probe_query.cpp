#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing probe -- QUERY")
{
    exec("bennu-field-device --f ../data/configs/ep/dnp3-server.xml >fd-server.out 2>&1 &");
    exec("bennu-field-device --f ../data/configs/ep/dnp3-client.xml >fd-client.out 2>&1 &");
    exec("bennu-probe --c query >probe.out 2>&1");
    exec("pkill -f bennu-field-device");
    std::string res("I: ACK\n\tbrkr\n");
    CHECK(exec("grep -e ACK -e brkr probe.out") == res);
}
