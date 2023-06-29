/**
   @brief This file will describe the definition of a IEC61850 Integer Status
    Common Data Class.
*/
#ifndef __IEC61850_INS__
#define __IEC61850_INS__

#include "bennu/devices/modules/comms/iec61850/protocol/data-object.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    /**
       @brief This class defines the INS CDC as described in
       IEC61850.
     */
    class ins : public data_object {
    public:
      ins() {}
      ins( string name ) : name_(name) {}

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

    private:
      string name_;
      uint32_t stVal_;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_INS__ */
