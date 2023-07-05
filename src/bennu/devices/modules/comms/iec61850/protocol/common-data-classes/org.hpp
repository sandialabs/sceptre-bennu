/**
   @brief This file will describe the definition of a IEC61850 Object Reference Setting
    Common Data Class.
*/
#ifndef __IEC61850_ORG__
#define __IEC61850_ORG__

/* stl includes */
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/common-acsi-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/data-object.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    /**
       @brief This class defines the ORG CDC as described in
       IEC61850.
     */
    class org : public data_object {
    public:
      org() {}
      org( string name ) : name_(name) {}

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

      void set_cbref( string name) { setSrcRef_ = name; }
      string get_cbref( void ) { return setSrcRef_; }

    private:
      string name_;
      string setSrcRef_;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_ORG__ */
