#ifndef bennu_FIELDDEVICES_IEC61850_IEC61850RTUDATAHANDLER
#define bennu_FIELDDEVICES_IEC61850_IEC61850RTUDATAHANDLER

#include <memory>

#include "bennu/devices/field-device/FieldDeviceDataHandler.hpp"

namespace bennu {
    namespace field_devices {
        namespace iec61850 {

            using boost::property_tree::ptree;

            class IEC61850RtuDataHandler: public FieldDeviceDataHandler
            {
            public:

                virtual bool handleFieldDeviceTreeData( const boost::property_tree::ptree& tree );


            protected:


            };

        }

    }

}
#endif // bennu_FIELDDEVICES_IEC61850_IEC61850RTUDATAHANDLER
