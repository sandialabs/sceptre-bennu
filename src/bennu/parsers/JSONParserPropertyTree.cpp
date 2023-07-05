#include "JSONParserPropertyTree.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <boost/property_tree/json_parser.hpp>

#include "bennu/parsers/Parser.hpp"

namespace bennu {
namespace parsers {

JSONParserPropertyTree::JSONParserPropertyTree()
{
}

bool JSONParserPropertyTree::load(const std::string& filename, boost::property_tree::ptree& tree)
{
    try
    {
        tree.clear();
        read_json(filename, tree);
        return true;
    }
    catch(std::exception& e)
    {
        std::cerr << "JSON load failed: " << e.what() << "!" << std::endl;
        return false;
    }
}

bool JSONParserPropertyTree::save(const std::string& filename, const boost::property_tree::ptree& tree)
{
    try
    {
        std::ofstream os(filename.c_str());
        write_json(os, tree);
        return true;
    }
    catch(std::exception& e)
    {
        std::cerr << "JSON save failed: " << e.what() << "!" << std::endl;
        return false;
    }
}

static bool init()
{
    std::shared_ptr<bennu::parsers::JSONParserPropertyTree> jpp(new bennu::parsers::JSONParserPropertyTree);
    bennu::parsers::Parser::the()->registerParser("json", jpp);

    return true;
}

static bool result = init();

} // namespace parsers
} // namespace bennu
