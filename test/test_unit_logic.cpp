#include "doctest.h"

#include <chrono>
#include <memory>
#include <sstream>

#include <boost/property_tree/ptree.hpp>

#include "bennu/devices/field-device/DataManager.hpp"

// No CMake wiring needed: build LogicModule and cparse straight into this test
#include "bennu/devices/modules/logic/LogicModule.cpp"
#include "bennu/devices/modules/logic/cparse/containers.cpp"
#include "bennu/devices/modules/logic/cparse/functions.cpp"
#include "bennu/devices/modules/logic/cparse/packToken.cpp"
#include "bennu/devices/modules/logic/cparse/shunting-yard.cpp"

using bennu::field_device::DataManager;
using bennu::logic::LogicModule;

namespace {
struct Rig
{
    std::shared_ptr<DataManager> dm = std::make_shared<DataManager>();
    LogicModule lm;

    Rig(const std::string& logic)
    {
        boost::property_tree::ptree t;
        t.put("logic", logic);
        lm.setDataManager(dm);
        REQUIRE(lm.handleTreeData(t));
    }

    void cycle() // same order as FieldDevice::scanCycle
    {
        lm.scanInputs();
        lm.scanLogic(100);
        dm->updateInternalData();
    }
};
} // namespace

TEST_CASE("internal-tag counter rule advances every cycle")
{
    Rig r("counter = counter + 1");
    r.dm->addInternalData<double>("counter", 0.0);
    r.dm->addAnalogTag("counter");
    for (int i = 1; i <= 5; i++)
    {
        r.cycle();
        CHECK(r.dm->getDataByTag<double>("counter") == i);
    }
}

TEST_CASE("longest tag wins and blank lines are ignored")
{
    Rig r("\n  out = foo + foobar  \n\n  flag = out > 3\n");
    r.dm->addInternalData<double>("foo", 1.0);
    r.dm->addInternalData<double>("foobar", 2.0);
    r.dm->addInternalData<double>("out", 0.0);
    r.dm->addInternalData<bool>("flag", false);
    r.dm->addAnalogTag("foo");
    r.dm->addAnalogTag("foobar");
    r.dm->addAnalogTag("out");
    r.dm->addBinaryTag("flag");
    r.cycle(); // out = 3, flag still sees old out (0)
    CHECK(r.dm->getDataByTag<double>("out") == 3.0);
    CHECK_FALSE(r.dm->getDataByTag<bool>("flag"));
    r.dm->addInternalData<double>("foo", 2.0);
    r.cycle(); // out = 4, flag sees out = 3
    CHECK(r.dm->getDataByTag<double>("out") == 4.0);
    r.cycle();
    CHECK(r.dm->getDataByTag<bool>("flag"));
}
