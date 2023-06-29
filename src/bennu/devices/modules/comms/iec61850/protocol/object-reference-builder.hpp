/**
   @brief This file defines a builder object for the object reference syntax used
   in the IEC61850 standard
*/
#ifndef __IEC61850_OBJECT_REFERENCE_BUILDER__
#define __IEC61850_OBJECT_REFERENCE_BUILDER__

/* stl includes */
#include <string>
#include <vector>

/* boost include */
#include <boost/spirit/include/karma.hpp>

namespace ccss_protocols {
  namespace iec61850 {

    using std::string;
    using std::vector;
    using std::back_insert_iterator;

    namespace karma = boost::spirit::karma;
    namespace ascii = boost::spirit::ascii;

    /**
       @brief Grammar that defines an IEC61850 object reference generator
    */
    template<typename OutputIterator>
    struct object_ref_builder : karma::grammar<OutputIterator, vector<string>()> {
      object_ref_builder();

      karma::rule<OutputIterator, vector<string>()> start;
      karma::rule<OutputIterator, string()> ld;
      karma::rule<OutputIterator, string()> ln;
      karma::rule<OutputIterator, string()> dobj_or_dattr;
    };

    namespace build {
      /** @brief Builds an object reference based on the input in the input vector

          @param in Input vector containing the strings to construct the object
          reference

          @return Contructed object reference based on the input strings
      */
      string object_reference( vector<string> const& in );

      /** @brief Constructs a GOOSE control-block reference

          This function takes the logical device name and the logical node name
          that the GOOSE control-block belongs to, and combines them with the
          value set in gocb_nameand constructs the complete GOOSE control-block
          reference.

          The return value can be the set value for the GoCBRef member of a
          gocb object.

          @param ld_name logical device name that the GOOSE control block belongs to

          @param ln_name logical node name that the GOOSE control block belongs to

          @param gocb_name GOOSE control block name

          @return GOOSE control block reference for this GOOSE control block.
      */
      string gocb_reference(string ld_name, string ln_name, string gocb_name);

      /** @brief Constructs a Dataset reference

          This function takes the logical device name and the logical node name
          that this dataset belongs to, and combines them with the value in
          dataset_name to construct the complete Dataset Reference.

          @param ld_name logical device name that the GOOSE control block belongs to

          @param ln_name logical node name that the GOOSE control block belongs to

          @param dataset_name Dataset name

          @return GOOSE control block reference for this GOOSE control block.
      */
      string dataset_reference(string ld_name, string ln_name, string dataset_name);
    } // namespace build

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_OBJECT_REFERENCE_BUILDER__ */
