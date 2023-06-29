/**
   @brief This file will describe the definition of a IEC61850 Controllable Single Point
    Common Data Class.
*/
#ifndef __IEC61850_SPC__
#define __IEC61850_SPC__

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/data-object.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    /**
       @brief This class defines the SPC CDC as described in
       IEC61850.
     */
    class spc : public data_object {
    public:
      spc() {}
      spc( string name ) : name_(name) {}

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

    private:
      string name_;
      bool ctlVal_;
      bool stVal_;
      model_t ctlModel_;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_SPC__ */
