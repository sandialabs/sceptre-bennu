/**
   @brief This file defines a parser for the object reference syntax used in the
   IEC61850 standard
*/
/* stl includes */
#include <map>
#include <string>
#include <vector>

/* boost includes */
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>

#include "bennu/devices/modules/comms/iec61850/protocol/common-data.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/data-attribute.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/data-object.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/logical-device.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/logical-node.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/object-reference-parser.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    /**
       @brief Grammar that implements the IEC61850 object reference parser
       generator defined in object-reference-parser.hpp
    */
    template <typename Iterator>
    object_ref_parser<Iterator>::object_ref_parser() :
      object_ref_parser<Iterator>::base_type(start) {

      ld = (+(ascii::alnum));
      ln = (+(ascii::alnum));
      dobj_or_dattr = (+(ascii::alnum));

      start
    = (ld >> qi::lit('/') >> ln >> (qi::lit('&') | qi::lit('.') | qi::lit('$')) >>
       (dobj_or_dattr % (qi::lit('&') | qi::lit('.'))));

    }
    /**************************************************************/

    namespace parse {
      bool object_reference( string ref, vector<string>& parsed ) {
        string::const_iterator
          ref_start_iter(ref.begin()), ref_end_iter(ref.end());

        object_ref_parser<string::const_iterator> parser;

        bool success =
          qi::parse(ref_start_iter, ref_end_iter, parser, parsed);

        return success;
      }
    } // namespace parser


    typedef std::map<string, logical_device> data_store_t;

    data_set* parse_data_set_ref( std::string ref, data_store_t& ds ) {
      string::const_iterator
    ref_start_i(ref.begin()),
    ref_end_i(ref.end());

      std::vector<string> parsed_objs;
      object_ref_parser<string::const_iterator> parser;

      bool success = qi::parse(ref_start_i, ref_end_i, parser, parsed_objs);
      if ( (!success) || (ref_start_i != ref_end_i)) {
    /* parsing failed */
    return NULL;
      }
      else {
    /* parsing success */
      }

      /* retrieve the referenced logical device */
      std::string ld_name = parsed_objs[0];
      if (!ds.count(ld_name)) {
    /*failed to find the reference name in the data store*/
    return NULL;
      }
      logical_device& ld = ds[ld_name];

      /* retrieve the referenced logical node */
      std::string ln_name = parsed_objs[1];
      if(!ld.logical_nodes.count(ln_name)) {
    /*failed to find the reference name in the data store*/
    return NULL;
      }
      logical_node& ln = ld.logical_nodes[ln_name];

      /* retrieve the referenced data-set reference */
      std::string dataset_ref = parsed_objs[2];
      if (!ln.data_sets.count(dataset_ref)) {
    /*failed to find the reference name in the data store*/
    return NULL;
      }

      return &(ln.data_sets[dataset_ref]);
    }

    data_attribute* parse_object_ref( std::string ref,  data_store_t& ds ) {
      string::const_iterator
    ref_start_i(ref.begin()),
    ref_end_i(ref.end());

      std::vector<string> parsed_objs;
      object_ref_parser<string::const_iterator> parser;

      bool success = qi::parse(ref_start_i, ref_end_i, parser, parsed_objs);
      if ( (!success) || (ref_start_i != ref_end_i)) {
    /* parsing failed */
    return NULL;
      }
      else {
    /* parsing success */
      }

      /* retrieve the referenced logical device */
      std::string ld_name = parsed_objs[0];
      if (!ds.count(ld_name)) {
    /*failed to find the reference name in the data store*/
    return NULL;
      }
      logical_device& ld = ds[ld_name];

      /* retrieve the referenced logical node */
      std::string ln_name = parsed_objs[1];
      if(!ld.logical_nodes.count(ln_name)) {
    /*failed to find the reference name in the data store*/
    return NULL;
      }
      logical_node& ln = ld.logical_nodes[ln_name];

      /* Retrieve the referenced data object */
      std::string dataobj_ref = parsed_objs[2];
      if (ln.data_objects.count(dataobj_ref)) {
    /*failed to find the reference name in the data store*/
    return NULL;
      }
      data_object& dataobj = ln.data_objects[dataobj_ref];

      /* Retrive the referenced sub-data object or data attribute.

     Iterate over the parsed objects in the container while searching the
     sub-data objects for the reference provided. Once the reference is not
     found in the sub_data_objects sub containers look for the current reference
     in the data attributes container of the last sub_data_objects container. */
      std::string obj_ref = parsed_objs[3];
      if (dataobj.sub_data_objects.count(obj_ref)) {
    common_data& sub_dobj = dataobj.sub_data_objects[obj_ref];

    for (uint8_t x=4; x < parsed_objs.size(); ++x ) {
      obj_ref = parsed_objs[x];

      if (sub_dobj.sub_data_objects.count(obj_ref)) {
        sub_dobj = sub_dobj.sub_data_objects[obj_ref];
      }
      else if (sub_dobj.data_attributes.count(obj_ref)) {
        /* return a pointer to the data store object instance */
        return &(sub_dobj.data_attributes[obj_ref]);
      }
      else {
        /*failed to find the reference name in the data store*/
        return NULL;
      }
    }
      }
      else if (dataobj.data_attributes.count(obj_ref)) {
    /* return a pointer to the data store object instance */
    return &(dataobj.data_attributes[obj_ref]);
      }
      else {
    /*failed to find the reference name in the data store*/
    return NULL;
      }

      /* Should never reach this point.  If it happens, there is an error in
     the parsing logic */
      return NULL;
    }

  } // namespace iec61850

} // namespace ccss_protocols
