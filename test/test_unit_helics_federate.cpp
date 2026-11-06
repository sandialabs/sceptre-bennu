#include "doctest.h"

#include <fstream>
#include <string>
#include <vector>

#include <helics/helics.hpp>
#include "deps/json.hpp"

using json = nlohmann::json;

namespace {

inline std::string findConfigFile(const std::string& relPath)
{
    const std::vector<std::string> candidates = {
        std::string(BENNU_SOURCE_DIR) + "/" + relPath};
    for (const auto& path : candidates)
    {
        std::ifstream f(path);
        if (f.good())
        {
            return path;
        }
    }
    return "";
}

} // namespace

TEST_CASE("HELICS: In-memory ValueFederate with CoreType::TEST co-simulation")
{
    auto cType = helics::core_type::TEST;
    if (!helics::isCoreTypeAvailable(cType))
    {
        if (helics::isCoreTypeAvailable(helics::core_type::INPROC))
        {
            cType = helics::core_type::INPROC;
        }
    }
    helics::FederateInfo fi(cType);
    fi.coreInitString = "-f 1 --autobroker";
    fi.autobroker = true;

    helics::ValueFederate vFed("test_inproc_fed", fi);
    auto& pubDouble = vFed.registerGlobalPublication<double>("power_mw");
    auto& subDouble = vFed.registerSubscription("power_mw");
    auto& pubBool = vFed.registerGlobalPublication<bool>("breaker_status");
    auto& subBool = vFed.registerSubscription("breaker_status");

    vFed.setProperty(helics_property_time_delta, 1.0);
    subDouble.setDefault(0.0);
    subBool.setDefault(false);

    vFed.enterExecutingMode();

    // Step 0: publish initial values
    pubDouble.publish(125.75);
    pubBool.publish(true);

    // Initial check before advance
    CHECK(subDouble.getValue<double>() == 0.0);
    CHECK(subBool.getValue<bool>() == false);

    // Advance time to 1.0
    auto granted = vFed.requestTime(1.0);
    CHECK(granted == 1.0);
    CHECK(doctest::Approx(subDouble.getValue<double>()).epsilon(0.001) == 125.75);
    CHECK(subBool.getValue<bool>() == true);

    // Step 1: publish updated values
    pubDouble.publish(250.50);
    pubBool.publish(false);

    granted = vFed.requestTime(2.0);
    CHECK(granted == 2.0);
    CHECK(doctest::Approx(subDouble.getValue<double>()).epsilon(0.001) == 250.50);
    CHECK(subBool.getValue<bool>() == false);

    vFed.finalize();
}

TEST_CASE("HELICS: Real JSON configuration parsing and schema validation")
{
    // 1. helics_test-ep-server.json
    std::string testEpPath = findConfigFile("data/configs/helics/helics_test-ep-server.json");
    REQUIRE_FALSE(testEpPath.empty());

    std::ifstream epFile(testEpPath);
    REQUIRE(epFile.is_open());
    json epCfg = json::parse(epFile);

    CHECK(epCfg["name"] == "helics-test-ep-server");
    CHECK(epCfg["terminate_on_error"] == true);
    CHECK(epCfg["only_update_on_change"] == true);

    REQUIRE(epCfg.contains("subscriptions"));
    CHECK(epCfg["subscriptions"].size() == 1);
    CHECK(epCfg["subscriptions"][0]["key"] == "helics-simulink-provider/Pump1.active");
    CHECK(epCfg["subscriptions"][0]["type"] == "bool");

    REQUIRE(epCfg.contains("publications"));
    CHECK(epCfg["publications"].size() == 3);

    REQUIRE(epCfg.contains("endpoints"));
    CHECK(epCfg["endpoints"].size() == 3);

    // 2. helics_simulink-provider.json
    std::string simProvPath = findConfigFile("data/configs/helics/helics_simulink-provider.json");
    REQUIRE_FALSE(simProvPath.empty());

    std::ifstream simFile(simProvPath);
    REQUIRE(simFile.is_open());
    json simCfg = json::parse(simFile);

    CHECK(simCfg["name"] == "helics-simulink-provider");
    REQUIRE(simCfg.contains("publications"));
    CHECK(simCfg["publications"].size() > 0);
    REQUIRE(simCfg.contains("endpoints"));
    CHECK(simCfg["endpoints"].size() > 0);
}

TEST_CASE("HELICS: Configuration error handling on missing and malformed JSON")
{
    // Missing file handling
    std::ifstream missingFile("nonexistent_helics_config_xyz.json");
    CHECK_FALSE(missingFile.is_open());

    // Malformed JSON parsing throws json::parse_error
    const std::string malformedJson = "{ \"name\": \"broken\", \"publications\": [ unclosed ";
    CHECK_THROWS_AS(
        [&]() {
            auto parsed = json::parse(malformedJson);
            (void)parsed;
        }(),
        json::parse_error);
}
