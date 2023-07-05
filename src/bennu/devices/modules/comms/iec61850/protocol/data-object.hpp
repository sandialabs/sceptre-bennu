/**
   @brief This file describes the Data Object Class
   as defined in IEC61850-7-2
 */
#ifndef __IEC61850_DATA_OBJECT_HPP__
#define __IEC61850_DATA_OBJECT_HPP__

/* stl includes */
#include <string>
#include <vector>

/* boost includes */
#include <boost/algorithm/string.hpp>

#include "bennu/devices/modules/comms/iec61850/protocol/common-data.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::string;
    using std::vector;

    class data_object : public common_data {
    public:
      data_object() :
    name_(), do_ref_(), opt_(), data_object_type_(get_id()) {}

      /* Services */
      void get_data_values( void );
      void set_data_values( void );
      void get_data_directory( void );
      void get_data_definition( void );

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

      void set_objref( string objref ) { do_ref_ = objref; }
      string get_objref( void ) { return do_ref_; }
    private:
      string name_;
      string do_ref_;
      optional_t opt_;

      string data_object_type_;
    };

  } // namespace iec61850

} // namespace ccss_protocols


#endif /* __IEC61850_DATA_OBJECT_HPP__ */
