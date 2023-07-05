#ifndef BENNU_PARSERS_JSONPARSERPROPERTYTREE_HPP
#define BENNU_PARSERS_JSONPARSERPROPERTYTREE_HPP

#include <boost/property_tree/ptree.hpp>

#include "bennu/parsers/ParserPropertyTree.hpp"

namespace bennu {
namespace parsers {

class JSONParserPropertyTree : public ParserPropertyTree
{
public:
    JSONParserPropertyTree();

    // Loads property_tree structure from the specified JSON file
    virtual bool load(const std::string& filename, boost::property_tree::ptree& tree);

    virtual bool save(const std::string& filename, const boost::property_tree::ptree& tree);

};

} // namespace parsers
} // namespace bennu

#endif // BENNU_PARSERS_JSONPARSERPROPERTYTREE_HPP
