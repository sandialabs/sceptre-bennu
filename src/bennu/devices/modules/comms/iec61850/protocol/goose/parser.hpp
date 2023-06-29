/**
   @brief This file defines a parser for data packed into an IEC61850 GOOSE
   'on-the-wire' packet
*/
#ifndef __IEC61850_GOOSE_PARSER__
#define __IEC61850_GOOSE_PARSER__

/* stl includes */
#include <cstdint>
#include <vector>

#define BOOST_SPIRIT_USE_PHOENIX_V3

/* boost includes */
#include <boost/spirit/include/qi.hpp>
#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"

/**@brief Boost fusion macro that allows spirit to parse data into
   a user-defined struct*/
BOOST_FUSION_ADAPT_STRUCT
(
 ccss_protocols::iec61850::basic_value_t,
 (std::uint8_t, type)
 (std::vector<std::uint8_t>, val)
 )

namespace ccss_protocols {

  namespace iec61850 {

    namespace goose {

      using std::vector;
      using std::uint8_t;

      namespace qi = boost::spirit::qi;

      /**
     @brief Grammar that defines an IEC61850 tuple-parser generator

     IEC61850 header fields and data into tuples that are of the form:

         tag -> length -> value

     The following grammar will generate a parser that is will parse
     the raw (off-the-wire) data into instances of the basic_type_t struct
     and store them in a vector
      */
      template <typename Iterator_T>
      struct goose_grammar :
    qi::grammar<Iterator_T, vector<basic_value_t>(), qi::locals<size_t> >
      {
    goose_grammar();

    qi::rule<Iterator_T, vector<basic_value_t>(), qi::locals<size_t> > start;
    qi::rule<Iterator_T, basic_value_t(), qi::locals<size_t> > protocol_tuple;
      };

      template <typename Iterator_T>
      goose_grammar<Iterator_T>::
      goose_grammar() :
    goose_grammar<Iterator_T>::base_type(start),
    start(),
    protocol_tuple()
      {
    start
      = (*protocol_tuple)
      ;

    protocol_tuple
      = (qi::byte_ >>
         qi::omit[qi::byte_[qi::_a = qi::_1]] >>
         qi::repeat(qi::_a)[qi::byte_])
      ;
      }

    } // namespace goose

  } // namespace iec61850

} // namespace css_protocols

#endif /* __IEC61850_GOOSE_PARSER__ */
