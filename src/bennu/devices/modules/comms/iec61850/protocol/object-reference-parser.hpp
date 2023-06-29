/**
   @brief This file defines a parser for the object reference syntax used in the
   IEC61850 standard
*/
#ifndef __IEC61850_OBJECT_REFERENCE_PARSER__
#define __IEC61850_OBJECT_REFERENCE_PARSER__

/* stl includes */
#include <map>
#include <string>
#include <vector>

/* boost includes */
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>

namespace ccss_protocols {
  namespace iec61850 {

    using std::map;
    using std::vector;
    using std::string;

    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;

    /**
       @brief Grammar that defines an IEC61850 object reference parser
       generator
    */
    template <typename Iterator>
    struct object_ref_parser : qi::grammar<Iterator, vector<string>()> {
      object_ref_parser();

      qi::rule<Iterator, vector<string>()> start;
      qi::rule<Iterator, string()> ld;
      qi::rule<Iterator, string()> ln;
      qi::rule<Iterator, string()> dobj_or_dattr;
    };
    /**************************************************************/

    namespace parse {
      bool object_reference( string ref, vector<string>& parsed );
    } // namespace parser

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_OBJECT_REFERENCE_PARSER__ */
