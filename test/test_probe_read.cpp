#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing probe -- READ")
{
    exec("bennu-test-ep-server --d 1 >ep.out 2>&1 &");
    exec("bennu-field-device --f ../data/configs/ep/dnp3-server.xml >fd-server.out 2>&1 &");
    exec("bennu-field-device --f ../data/configs/ep/dnp3-client.xml >fd-client.out 2>&1 &");
    exec("sleep 10");
    exec("bennu-probe --c read --t brkr >probe.out 2>&1");
    exec("bennu-probe --c read --t load-power >>probe.out 2>&1");
    exec("pkill -f bennu-test-ep-server");
    exec("pkill -f bennu-field-device");
    std::string res("I: ACK\n\tbrkr:true\nI: ACK\n\tload-power:400.549988\n");
    CHECK(exec("grep -e ACK -e brkr -e load-power probe.out") == res);
}
