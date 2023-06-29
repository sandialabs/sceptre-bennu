#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing brash")
{
    exec("echo -e 'help\nexit' | bennu-brash >brash.out 2>&1");
    std::string res("1\n");
    CHECK(exec("grep -c 'SCEPTRE Field-Device FW' brash.out") == res);
}
