#ifndef bennu_FIELD_DEVICES_IEC61850_IEC61850RTU
#define bennu_FIELD_DEVICES_IEC61850_IEC61850RTU

#include <memory>
#include <sstream>
#include <thread>

#include "bennu/devices/field-device/FieldDevice.hpp"
#include "bennu/devices/modules/comms/iec61850/device/outstation.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/goose/data-set.hpp"

namespace bennu {
    namespace field_devices {
        namespace iec61850 {

            class IEC61850RTU : public bennu::field_devices::FieldDevice
            {
            public:

                IEC61850RTU( const std::string& name );
                virtual ~IEC61850RTU();

                void setInterface( const std::string& interface );
                void setConfigurationFile( const std::string& configuration );

                virtual bool startDevice();

                bool addRegister( const std::string& provider, const boost::uint16_t address, const gryffin::Device& device );

                void process( ccss_protocols::iec61850::goose::data_set& ds );


            protected:


            private:

                boost::shared_ptr<ccss_devices::iec61850::goose::outstation> mOutstation;

                std::string mInterface;
                std::string mConfigurationFile;
                bool mHandlingRisingEdge;

                std::map<boost::uint16_t, std::pair<size_t, std::string> > mRegisters;


                IEC61850RTU(const IEC61850RTU&);
                IEC61850RTU& operator =(const IEC61850RTU&);

            };
        }
    }
}


#endif  // bennu_FIELD_DEVICES_IEC61850_IEC61850RTU
