#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing probe -- WRITE")
{
    exec("bennu-test-ep-server --d 1 >ep.out 2>&1 &");
    exec("bennu-field-device --f ../data/configs/ep/dnp3-server.xml >fd-server.out 2>&1 &");
    exec("bennu-field-device --f ../data/configs/ep/dnp3-client.xml >fd-client.out 2>&1 &");
    exec("sleep 10");
    exec("bennu-probe --c write --t load-breaker-toggle --s false >probe.out 2>&1");
    exec("bennu-probe --c write --t load-mw-setpoint --v 999 >>probe.out 2>&1");
    std::string res("I: ACK\n\tWrote tag load-breaker-toggle -- false\nI: ACK\n\tWrote tag load-mw-setpoint -- 999\n");
    CHECK(exec("grep -e ACK -e load-breaker-toggle -e load-mw-setpoint probe.out") == res);
    exec("sleep 5");
    exec("bennu-probe --c read --t brkr >probe.out 2>&1");
    exec("bennu-probe --c read --t load-power >>probe.out 2>&1");
    exec("pkill -f bennu-test-ep-server");
    exec("pkill -f bennu-field-device");
    res = "I: ACK\nReply:\n\tbrkr:false\nI: ACK\nReply:\n\tload-power:999.000000\n";
    CHECK(exec("grep -v 'Client connect' probe.out") == res);
}
