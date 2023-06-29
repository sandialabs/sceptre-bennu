/**
   @brief This file will describe a data attribute as defined in
   IEC61850-7-2 (Table 19)
*/
#ifndef __IEC61850_DATA_ATTRIBUTE__
#define __IEC61850_DATA_ATTRIBUTE__

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/common-acsi-types.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    /**@brief defined in IEC61850-7-2 (Table 20)*/
    struct functional_constraint_t {
      enum value {
    ST,
    MX,
    SP,
    SV,
    CF,
    DC,
    SG,
    SE,
    SR,
    OR,
    BL,
    EX,
    XX
      };
    };

    struct optional_t {
      enum value {
    M, /* mandatory */
    O, /* optional */
    C  /* conditional */
      };
    };

    /**@brief defined in IEC61850-7-2 (Table 19)*/
    class data_attribute {
    public:
      data_attribute() {}
      data_attribute( string name );

      void set_name( string name ) { name_ = name; }
      string get_name( void ) { return name_; }

      void set_objref( string objref ) { da_ref_ = objref; }
      string get_objref( void ) { return da_ref_; }

      void set_value( basic_value_t val ) { value_ = val; }
      basic_value_t get_value(void) { return value_; }

    private:
      string name_;
      string da_ref_;

      functional_constraint_t fc_;
      trigger_conditions_t trg_op_;
      optional_t opt_;

      basic_value_t value_;
    };

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_DATA_ATTRIBUTE__ */
