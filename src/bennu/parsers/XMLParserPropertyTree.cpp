#include "XMLParserPropertyTree.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <boost/property_tree/xml_parser.hpp>

#include "bennu/parsers/Parser.hpp"

namespace bennu {
namespace parsers {

XMLParserPropertyTree::XMLParserPropertyTree()
{
}

bool XMLParserPropertyTree::load(const std::string& filename, boost::property_tree::ptree& tree)
{
    try
    {
        tree.clear();
        read_xml(filename, tree);
        return true;
    }
    catch(std::exception& e)
    {
        std::cerr << "XML load failed: " << e.what() << "!" << std::endl;
        return false;
    }
}

bool XMLParserPropertyTree::save(const std::string& filename, const boost::property_tree::ptree& tree)
{
    try
    {
        std::ofstream os(filename.c_str());
        boost::property_tree::xml_writer_settings<std::string> xws(' ', 4);
        write_xml(os, tree, xws);
        return true;
    }
    catch(std::exception& e)
    {
        std::cerr << "XML save failed: " << e.what() << "!" << std::endl;
        return false;
    }
}

static bool init()
{
    std::shared_ptr<bennu::parsers::XMLParserPropertyTree> xpp(new bennu::parsers::XMLParserPropertyTree);
    bennu::parsers::Parser::the()->registerParser("xml", xpp);
    return true;
}

static bool result = init();

} // namespace parsers
} // namespace bennu
