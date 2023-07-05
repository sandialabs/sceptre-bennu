/**
   @brief This file defines a builder object for the object reference syntax used
   in the IEC61850 standard as defined in object-reference-builder.hpp
*/
#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/object-reference-builder.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    template<typename OutputIterator>
    object_ref_builder<OutputIterator>::object_ref_builder() :
      object_ref_builder<OutputIterator>::base_type(start) {

      ld = (karma::char_("a-zA-Z_") << *karma::char_("a-zA-Z_0-9"));
      ln = (karma::char_("a-zA-Z_") << *karma::char_("a-zA-Z_0-9"));
      dobj_or_dattr = (karma::char_("a-zA-Z_") << *karma::char_("a-zA-Z_0-9"));

      start
    = (ld << karma::lit('/') << ln << karma::lit('$') <<
           (dobj_or_dattr % karma::lit('.')));

    }

    namespace build {
      string object_reference( vector<string> const& in ) {
        string output_str;
        back_insert_iterator<string> output_iter(output_str);

        object_ref_builder< back_insert_iterator<string> > generator;

        karma::generate
          (
           output_iter, // destination: output iterator
           generator,   // the generator
           in           // the data to output
           );

        return output_str;
      }

      string gocb_reference(string ld_name, string ln_name, string gocb_name) {
        if ((gocb_name.size() <= 0) || (ld_name.size() <= 0) || (ln_name.size() <= 0)) {
          throw ccss_protocols::Exception
            ("Cannot build the GOOSE control reference: uninitialized parameters");
        }

        vector<string> input_strings;
        input_strings.push_back(ld_name);
        input_strings.push_back(ln_name);
        input_strings.push_back(gocb_name);

        return (build::object_reference( input_strings ));
      }

      string dataset_reference(string ld_name, string ln_name, string dataset_name) {
        if ((dataset_name.size() <= 0) || (ld_name.size() <= 0) || (ln_name.size() <= 0)) {
          throw ccss_protocols::Exception
            ("Cannot build the Dataset reference: uninitialized parameters");
        }

        vector<string> input_strings;
        input_strings.push_back(ld_name);
        input_strings.push_back(ln_name);
        input_strings.push_back(dataset_name);

        return (build::object_reference( input_strings ));
      }
    } // namespace build

  } // namespace iec61850

} // namespace ccss_protocols
