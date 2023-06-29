/**
   @brief Definitions of the common ACSI types defined in
   IEC61850 (Sec. 6.1.2.1)
 */
#ifndef __IEC61850_COMMON_ACSI_TYPES__
#define __IEC61850_COMMON_ACSI_TYPES__

/* stl includes */
#include <string>

namespace ccss_protocols {
  namespace iec61850 {

    /**@brief defined in IEC61850-7-2 (Sec. 6.1.2.11)*/
    struct trigger_conditions_t {
      bool data_change;
      bool quality_change;
      bool data_update;
      bool integrity;
      bool general_interrogation;
    };

  } // namespace iec61850

} // namespace ccss_protocols


#endif /* __IEC61850_COMMON_ACSI_TYPES__ */
