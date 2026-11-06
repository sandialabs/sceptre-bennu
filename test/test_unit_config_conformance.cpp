#include "doctest.h"

#include <fstream>
#include <string>
#include <vector>

#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/ptree.hpp>

#include "bennu/parsers/JSONParserPropertyTree.hpp"
#include "bennu/parsers/Parser.hpp"
#include "bennu/parsers/XMLParserPropertyTree.hpp"

using bennu::parsers::JSONParserPropertyTree;
using bennu::parsers::Parser;
using bennu::parsers::XMLParserPropertyTree;

namespace {

inline std::string resolveConfigPath(const std::string& relPath)
{
    const std::vector<std::string> searchRoots = {
        std::string(BENNU_SOURCE_DIR) + "/data/configs/"};
    for (const auto& root : searchRoots)
    {
        std::string candidate = root + relPath;
        std::ifstream f(candidate);
        if (f.good())
        {
            return candidate;
        }
    }
    return "";
}

} // namespace

TEST_CASE("Batch Config Conformance: Electric Power (EP) XML configurations")
{
    const std::vector<std::string> epConfigs = {
        "ep/bacnet-server.xml",
        "ep/bacnet-client.xml",
        "ep/dnp3-server.xml",
        "ep/dnp3-client.xml",
        "ep/modbus-server.xml",
        "ep/modbus-client.xml",
        "ep/modbus-serial-server.xml",
        "ep/modbus-serial-client.xml",
        "ep/iec60870-5-104-server.xml",
        "ep/iec60870-5-104-server-sbo.xml",
        "ep/iec60870-5-104-client.xml",
        "ep/sunspec-server.xml"};

    XMLParserPropertyTree xmlParser;

    for (const auto& cfg : epConfigs)
    {
        CAPTURE(cfg);
        INFO("Checking config file: " << cfg);
        std::string fullPath = resolveConfigPath(cfg);
        REQUIRE(!fullPath.empty());

        // 1. Direct XMLParserPropertyTree validation
        boost::property_tree::ptree pt;
        bool loaded = false;
        CHECK_NOTHROW(loaded = xmlParser.load(fullPath, pt));
        CHECK(loaded);
        CHECK_FALSE(pt.empty());

        // Must contain root 'SCEPTRE.field-device' element
        CHECK_NOTHROW(pt.get_child("SCEPTRE.field-device"));

        // 2. Dispatcher Parser::the()->load validation
        CHECK_NOTHROW(Parser::the()->load(fullPath));
    }
}

TEST_CASE("Batch Config Conformance: Batch Process (BP) XML configurations")
{
    const std::vector<std::string> bpConfigs = {
        "bp/dnp3-server.xml",
        "bp/dnp3-client.xml",
        "bp/modbus-server.xml",
        "bp/modbus-client.xml"};

    XMLParserPropertyTree xmlParser;

    for (const auto& cfg : bpConfigs)
    {
        CAPTURE(cfg);
        INFO("Checking config file: " << cfg);
        std::string fullPath = resolveConfigPath(cfg);
        REQUIRE(!fullPath.empty());

        boost::property_tree::ptree pt;
        bool loaded = false;
        CHECK_NOTHROW(loaded = xmlParser.load(fullPath, pt));
        CHECK(loaded);
        CHECK_FALSE(pt.empty());

        CHECK_NOTHROW(pt.get_child("SCEPTRE.field-device"));
        CHECK_NOTHROW(Parser::the()->load(fullPath));
    }
}

TEST_CASE("Batch Config Conformance: HELICS JSON configurations")
{
    const std::vector<std::string> helicsConfigs = {
        "helics/helics_test-ep-server.json",
        "helics/helics_simulink-provider.json",
        "helics/helics_provider-broker.json"};

    JSONParserPropertyTree jsonParser;

    for (const auto& cfg : helicsConfigs)
    {
        CAPTURE(cfg);
        INFO("Checking config file: " << cfg);
        std::string fullPath = resolveConfigPath(cfg);
        REQUIRE(!fullPath.empty());

        boost::property_tree::ptree pt;
        bool loaded = false;
        CHECK_NOTHROW(loaded = jsonParser.load(fullPath, pt));
        CHECK(loaded);
        CHECK_FALSE(pt.empty());

        // Root JSON should have 'name' or 'coreType' or similar properties
        bool hasRootProp = (pt.count("name") > 0 || pt.count("broker") > 0);
        CHECK(hasRootProp);
        CHECK_NOTHROW(Parser::the()->load(fullPath));
    }
}
