#include "bennu/devices/field-device/FieldDeviceManager.hpp"
#include "bennu/devices/modules/comms/iec61850/goose/IEC61850RtuDataHandler.hpp"
#include "bennu/devices/modules/comms/iec61850/goose/IEC61850RTU.hpp"

using namespace bennu::field_devices::iec61850;
using boost::property_tree::ptree_bad_path;
using boost::property_tree::ptree_error;

namespace sk = bennu::gryffin;

bool IEC61850RtuDataHandler::handleFieldDeviceTreeData( const boost::property_tree::ptree& tree )
{
    try
    {
        std::pair<ptree::const_assoc_iterator, ptree::const_assoc_iterator> rtus = tree.equal_range( "iec61850-rtu" );
        for ( ptree::const_assoc_iterator iter = rtus.first; iter != rtus.second; ++iter )
        {
            std::string name = iter->second.get<std::string>( "name" );
            // Since it's an electric power rtu, know the update type and the filter type.
            std::shared_ptr<iec61850::IEC61850RTU> rtu( new iec61850::IEC61850RTU( name ) );

            std::string configuration = iter->second.get<std::string>( "configuration-file" );
            rtu->setConfigurationFile( configuration );
            std::string interface = iter->second.get<std::string>( "interface" );
            rtu->setInterface( interface );

            std::pair<ptree::const_assoc_iterator, ptree::const_assoc_iterator> registers = iter->second.equal_range( "entry" );
            for ( ptree::const_assoc_iterator rIter = registers.first; rIter != registers.second; ++rIter )
            {
                size_t address = rIter->second.get<size_t>( "register-address" );
                ptree deviceTree = rIter->second.get_child( "device" );
                sk::Device device;
                if ( !parseDevice( deviceTree, device ) )
                {
                    std::cerr << "ERROR: iec61850 device mapping for " << device.mName << " for field " << device.mField << " have not been configured for this infrastructure" << std::endl;
                    continue;
                }
                rtu->addRegister( provider, address, device );
            }

            mFieldDevices.push_back( rtu );

        }

        return true;
    }
    catch ( ptree_bad_path& e )
    {
        std::cerr << "ERROR: Format was incorrect in iec61850 RTU setup: " << e.what() << std::endl;
        mFieldDevices.clear();
        return false;
    }
    catch ( ptree_error& e )
    {
        std::cerr << "ERROR: There was a problem parsing iec61850 RTU setup: " << e.what() << std::endl;
        mFieldDevices.clear();
        return false;
    }

}


static bool iec61850RTUInit()
{
    std::shared_ptr<bennu::field_devices::iec61850::IEC61850RtuDataHandler> iec61850RtuHandler( new bennu::field_devices::iec61850::IEC61850RtuDataHandler );
    bennu::infrastructures::InfrastructureDataManager::the()->addFieldDeviceDataHandler( iec61850RtuHandler );
    return true;
}


static bool result = iec61850RTUInit();
