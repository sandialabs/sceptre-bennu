#ifndef BENNU_PARSERS_PARSERPROPERTYTREE_HPP
#define BENNU_PARSERS_PARSERPROPERTYTREE_HPP

#include <string>

#include <boost/property_tree/ptree.hpp>

namespace bennu {
namespace parsers {

class ParserPropertyTree
{
public:
    virtual bool load(const std::string& filename, boost::property_tree::ptree& tree) = 0;

    virtual bool save(const std::string& filename, const boost::property_tree::ptree& tree) = 0;

};

} // namespace parsers
} // namespace bennu

#endif // BENNU_PARSERS_PARSERPROPERTYTREE_HPP
