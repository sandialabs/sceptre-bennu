#include "doctest.h"
#include <string>

extern std::string exec(const char*);

TEST_CASE("testing probe -- no args")
{
    std::string res("Error: you must define a command: query, read, or write.\n");
    CHECK(exec("bennu-probe") == res);
}
