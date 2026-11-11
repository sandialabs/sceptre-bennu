#include "doctest.h"
#include <string>

extern std::string exec(const char*);

// installed binaries not otherwise exercised by ctest must at least start and print usage
static std::string helpCheck(const std::string& bin)
{
    std::string cmd = "out=$(" + bin + " --help 2>&1); rc=$?; "
                                       "echo \"rc=$rc\"; echo \"$out\" | grep -q -e '--help' && echo usage";
    return exec(cmd.c_str());
}

TEST_CASE("testing --help -- bennu-watcherd")
{
    CHECK(helpCheck("bennu-watcherd") == "rc=0\nusage\n");
}

TEST_CASE("testing --help -- bennu-simulink-provider")
{
    CHECK(helpCheck("bennu-simulink-provider") == "rc=0\nusage\n");
}

TEST_CASE("testing --help -- bennu-field-deviced")
{
    CHECK(helpCheck("bennu-field-deviced") == "rc=0\nusage\n");
}

TEST_CASE("testing --help -- bennu-simulink-provider-helics")
{
    CHECK(helpCheck("bennu-simulink-provider-helics") == "rc=0\nusage\n");
}
