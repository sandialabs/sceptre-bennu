#include "doctest.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/property_tree/ptree.hpp>

#include "bennu/parsers/JSONParserPropertyTree.hpp"
#include "bennu/parsers/Parser.hpp"
#include "bennu/parsers/XMLParserPropertyTree.hpp"
#include "deps/json.hpp"

using json = nlohmann::json;
using bennu::parsers::JSONParserPropertyTree;
using bennu::parsers::Parser;
using bennu::parsers::XMLParserPropertyTree;

namespace {

struct ScopedFileCleanup
{
    std::string path;
    explicit ScopedFileCleanup(std::string p) :
        path(std::move(p))
    {}
    ~ScopedFileCleanup()
    {
        std::remove(path.c_str());
    }
};

} // namespace

TEST_CASE("JSONParserPropertyTree: load nested trees, arrays, numeric, string, boolean")
{
    const std::string testFile = "/tmp/test_unit_parser_sample.json";
    ScopedFileCleanup cleanup(testFile);
    {
        std::ofstream out(testFile);
        out << "{\n"
            << "  \"str_val\": \"hello world\",\n"
            << "  \"int_val\": 42,\n"
            << "  \"double_val\": 3.1415,\n"
            << "  \"bool_val\": true,\n"
            << "  \"nested\": {\n"
            << "    \"child_str\": \"bennu\",\n"
            << "    \"child_int\": 999\n"
            << "  },\n"
            << "  \"items\": [\"first\", \"second\", \"third\"]\n"
            << "}\n";
    }

    JSONParserPropertyTree parser;
    boost::property_tree::ptree pt;
    REQUIRE(parser.load(testFile, pt));

    CHECK(pt.get<std::string>("str_val") == "hello world");
    CHECK(pt.get<int>("int_val") == 42);
    CHECK(doctest::Approx(pt.get<double>("double_val")) == 3.1415);
    CHECK(pt.get<bool>("bool_val") == true);
    CHECK(pt.get<std::string>("nested.child_str") == "bennu");
    CHECK(pt.get<int>("nested.child_int") == 999);

    // Array elements in boost::property_tree have empty keys
    std::vector<std::string> items;
    for (const auto& item : pt.get_child("items"))
    {
        items.push_back(item.second.data());
    }
    CHECK(items == std::vector<std::string>{"first", "second", "third"});
}

TEST_CASE("JSONParserPropertyTree: save ptree to JSON and roundtrip integrity")
{
    const std::string saveFile = "/tmp/test_unit_parser_roundtrip.json";
    ScopedFileCleanup cleanup(saveFile);

    boost::property_tree::ptree ptOut;
    ptOut.put("system.name", "sceptre");
    ptOut.put("system.version", 2);
    ptOut.put("system.rate", 10.5);
    ptOut.put("active", true);

    JSONParserPropertyTree parser;
    REQUIRE(parser.save(saveFile, ptOut));

    boost::property_tree::ptree ptIn;
    REQUIRE(parser.load(saveFile, ptIn));

    CHECK(ptIn.get<std::string>("system.name") == "sceptre");
    CHECK(ptIn.get<int>("system.version") == 2);
    CHECK(doctest::Approx(ptIn.get<double>("system.rate")) == 10.5);
    CHECK(ptIn.get<bool>("active") == true);
}

TEST_CASE("JSONParserPropertyTree: malformed syntax and missing file handling")
{
    JSONParserPropertyTree parser;
    boost::property_tree::ptree pt;

    // Nonexistent file
    CHECK_FALSE(parser.load("/tmp/nonexistent_file_12345.json", pt));

    // Malformed JSON syntax
    const std::string badFile = "/tmp/test_unit_parser_bad.json";
    ScopedFileCleanup cleanup(badFile);
    {
        std::ofstream out(badFile);
        out << "{ broken_json: [ unclosed ";
    }
    CHECK_FALSE(parser.load(badFile, pt));
}

TEST_CASE("JSONParserPropertyTree: empty file handling")
{
    JSONParserPropertyTree parser;
    boost::property_tree::ptree pt;

    const std::string emptyFile = "/tmp/test_unit_parser_empty.json";
    ScopedFileCleanup cleanup(emptyFile);
    {
        std::ofstream out(emptyFile);
        // empty file (0 bytes)
    }
    CHECK_FALSE(parser.load(emptyFile, pt));
}

TEST_CASE("XMLParserPropertyTree: load XML with attributes and nested tags")
{
    const std::string testFile = "/tmp/test_unit_parser_sample.xml";
    ScopedFileCleanup cleanup(testFile);
    {
        std::ofstream out(testFile);
        out << "<device id=\"dev101\" enabled=\"true\">\n"
            << "  <settings timeout=\"500\">\n"
            << "    <host>127.0.0.1</host>\n"
            << "    <port>20000</port>\n"
            << "  </settings>\n"
            << "  <points>\n"
            << "    <point name=\"pt1\">100</point>\n"
            << "    <point name=\"pt2\">200</point>\n"
            << "  </points>\n"
            << "</device>\n";
    }

    XMLParserPropertyTree parser;
    boost::property_tree::ptree pt;
    REQUIRE(parser.load(testFile, pt));

    CHECK(pt.get<std::string>("device.<xmlattr>.id") == "dev101");
    CHECK(pt.get<std::string>("device.<xmlattr>.enabled") == "true");
    CHECK(pt.get<int>("device.settings.<xmlattr>.timeout") == 500);
    CHECK(pt.get<std::string>("device.settings.host") == "127.0.0.1");
    CHECK(pt.get<int>("device.settings.port") == 20000);

    std::vector<std::pair<std::string, int>> points;
    for (const auto& node : pt.get_child("device.points"))
    {
        if (node.first == "point")
        {
            std::string name = node.second.get<std::string>("<xmlattr>.name");
            int val = node.second.get_value<int>();
            points.push_back({name, val});
        }
    }
    REQUIRE(points.size() == 2);
    CHECK(points[0].first == "pt1");
    CHECK(points[0].second == 100);
    CHECK(points[1].first == "pt2");
    CHECK(points[1].second == 200);
}

TEST_CASE("XMLParserPropertyTree: save ptree to XML and roundtrip integrity")
{
    const std::string saveFile = "/tmp/test_unit_parser_roundtrip.xml";
    ScopedFileCleanup cleanup(saveFile);

    boost::property_tree::ptree ptOut;
    ptOut.put("config.<xmlattr>.version", "1.0");
    ptOut.put("config.server.ip", "192.168.1.10");
    ptOut.put("config.server.port", 8080);

    XMLParserPropertyTree parser;
    REQUIRE(parser.save(saveFile, ptOut));

    boost::property_tree::ptree ptIn;
    REQUIRE(parser.load(saveFile, ptIn));

    CHECK(ptIn.get<std::string>("config.<xmlattr>.version") == "1.0");
    CHECK(ptIn.get<std::string>("config.server.ip") == "192.168.1.10");
    CHECK(ptIn.get<int>("config.server.port") == 8080);
}

TEST_CASE("XMLParserPropertyTree: malformed XML and missing file handling")
{
    XMLParserPropertyTree parser;
    boost::property_tree::ptree pt;

    // Nonexistent file
    CHECK_FALSE(parser.load("/tmp/nonexistent_file_xml_12345.xml", pt));

    // Malformed XML (mismatched tags)
    const std::string badFile = "/tmp/test_unit_parser_bad.xml";
    ScopedFileCleanup cleanup(badFile);
    {
        std::ofstream out(badFile);
        out << "<root><unclosed_tag>data</root>";
    }
    CHECK_FALSE(parser.load(badFile, pt));
}

TEST_CASE("XMLParserPropertyTree: empty file handling")
{
    XMLParserPropertyTree parser;
    boost::property_tree::ptree pt;

    const std::string emptyFile = "/tmp/test_unit_parser_empty.xml";
    ScopedFileCleanup cleanup(emptyFile);
    {
        std::ofstream out(emptyFile);
        // empty file (0 bytes)
    }
    // Boost read_xml parses an empty stream as an empty tree without throwing
    CHECK(parser.load(emptyFile, pt));
    CHECK(pt.empty());
}

TEST_CASE("Parser dispatcher: extension detection, missing parser, and TreeDataHandler dispatch")
{
    Parser* p = Parser::the();
    REQUIRE(p != nullptr);

    // No extension
    CHECK_FALSE(p->load("no_extension_file"));

    // Unregistered extension
    CHECK_FALSE(p->load("some_file.unregistered_ext"));

    // Missing file with valid extension
    CHECK_FALSE(p->load("nonexistent_file_xyz.xml"));

    // Create a real XML configuration in /tmp and register a handler
    const std::string testXml = "/tmp/test_unit_parser_dispatch.xml";
    ScopedFileCleanup cleanup(testXml);
    {
        std::ofstream out(testXml);
        out << "<system>\n"
            << "  <test-module name=\"sample_mod\">\n"
            << "    <rate>50</rate>\n"
            << "  </test-module>\n"
            << "</system>\n";
    }

    bool handlerInvoked = false;
    std::string receivedElement;
    int receivedRate = 0;

    p->registerTreeDataHandler("test-module", [&](const std::string& elem, const boost::property_tree::ptree& tree) -> bool {
        handlerInvoked = true;
        receivedElement = elem;
        receivedRate = tree.get<int>("rate");
        return true;
    });

    CHECK(p->load(testXml));
    CHECK(handlerInvoked);
    CHECK(receivedElement == "test-module");
    CHECK(receivedRate == 50);
}

TEST_CASE("nlohmann/json direct serialization: STL containers")
{
    // std::vector
    std::vector<int> intVec = {10, 20, 30, 40};
    json jVec = intVec;
    CHECK(jVec.dump() == "[10,20,30,40]");
    std::vector<int> deserializedVec = jVec.get<std::vector<int>>();
    CHECK(deserializedVec == intVec);

    // std::map
    std::map<std::string, double> mapData = {{"a", 1.25}, {"b", 2.50}};
    json jMap = mapData;
    CHECK(jMap["a"] == 1.25);
    CHECK(jMap["b"] == 2.50);
    auto deserializedMap = jMap.get<std::map<std::string, double>>();
    CHECK(deserializedMap == mapData);

    // std::unordered_map
    std::unordered_map<std::string, bool> umData = {{"flag1", true}, {"flag2", false}};
    json jUm = umData;
    CHECK(jUm["flag1"] == true);
    CHECK(jUm["flag2"] == false);
    auto deserializedUm = jUm.get<std::unordered_map<std::string, bool>>();
    CHECK(deserializedUm == umData);
}

TEST_CASE("nlohmann/json: parsing, value retrieval, and exception handling")
{
    // String parsing
    std::string jsonStr = "{\"title\": \"Bennu\", \"count\": 7, \"flag\": true}";
    json j = json::parse(jsonStr);
    CHECK(j["title"] == "Bennu");
    CHECK(j["count"] == 7);
    CHECK(j["flag"] == true);

    // Stream parsing
    std::istringstream stream("{\"temperature\": 98.6}");
    json jStream;
    stream >> jStream;
    CHECK(doctest::Approx(jStream["temperature"].get<double>()) == 98.6);

    // value("key", default)
    CHECK(j.value("count", 0) == 7);
    CHECK(j.value("missing_int", 999) == 999);
    CHECK(j.value("missing_str", std::string("default_val")) == "default_val");

    // json::parse_error exception handling
    CHECK_THROWS_AS(json::parse("{ incomplete_json "), json::parse_error);
    CHECK_THROWS_AS(json::parse("invalid text"), json::parse_error);
}
