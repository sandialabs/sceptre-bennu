/**
   @brief This file will describe the definition of a IEC61850 Controllable Double Point
    Common Data Class.
*/
#ifndef __IEC61850_DPC__
#define __IEC61850_DPC__

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/data-object.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    /**
       @brief This class defines the DPC CDC as described in
       IEC61850.
     */
    class dpc : public data_object {
    public:
      dpc() {}
      dpc( string name ) : name_(name) {}

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

    private:
      string name_;
      bool ctlVal_;
      status_t stVal_;
      model_t ctlModel_;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_DPC__ */
