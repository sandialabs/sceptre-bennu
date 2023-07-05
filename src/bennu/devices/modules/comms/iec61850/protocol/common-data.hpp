/**
   @brief This file describes the General Common Data Class (GenCommonDataClass)
   as defined in IEC61850-7-2
 */
#ifndef __IEC61850_COMMON_DATA_HPP__
#define __IEC61850_COMMON_DATA_HPP__

/* stl includes */
#include <map>
#include <string>
#include <vector>

/* boost includes */
#include <boost/algorithm/string.hpp>

#include "bennu/devices/modules/comms/iec61850/protocol/data-attribute.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::map;
    using std::pair;
    using std::vector;
    using std::string;

    class common_data {
    public:
      common_data() {}

      void set_id( string id ) { cdc_id_ = id; boost::to_upper(cdc_id_); }
      string get_id( void ) { return cdc_id_; }

      map<string, common_data> sub_data_objects;
      map<string, data_attribute> data_attributes;

    private:
      string cdc_id_;
    };

  } // namespace iec61850

} // namespace ccss_protocols


#endif /* __IEC61850_COMMON_DATA_HPP__ */
