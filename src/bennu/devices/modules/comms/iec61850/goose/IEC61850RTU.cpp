#include "IEC61850RTU.hpp"

#include <iostream>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/object-reference-builder.hpp"

namespace bennu_61850 = bennu::field_devices::iec61850;

namespace protocols = ccss_protocols::iec61850;
namespace devices = ccss_devices::iec61850::goose;
namespace sk = bennu::gryffin;


bennu_61850::IEC61850RTU::IEC61850RTU( const std::string& name ) :
    bennu::field_devices::FieldDevice( name ),
    mHandlingRisingEdge( false )
{
}


bennu_61850::IEC61850RTU::~IEC61850RTU()
{
    mOutstation.reset();
}


void bennu_61850::IEC61850RTU::setInterface( const std::string& interface )
{
    mInterface = interface;
}


void bennu_61850::IEC61850RTU::setConfigurationFile( const std::string& configuration )
{
    mConfigurationFile = configuration;
}


bool bennu_61850::IEC61850RTU::startOutstation()
{
    //std::string ld_name( "GOOSE_1CFG" );
    //std::string ln_name( "LLN0" );
    //uint8_t dst_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    //uint8_t dst_mac[] = { 0x00, 0x30, 0xA7, 0x05, 0xF3, 0x96 };

    //devices::gocb CB4( "UndervoltageBit", "FLAIL" );
    //CB4.GoCBRef(build::gocb_reference(ld_name, ln_name, CB4.GoCBName()));
    //CB4.GoCBRef( "GOOSE_1CFG/LLN0$GO$UndervoltageBit" );
    //CB4.goEna( true );
    //CB4.goID( "GOOSE_1" );
    //CB4.DatSet( ccss_protocols::iec61850::build::dataset_reference( ld_name, ln_name, CB4.dset.name() ) );
    //CB4.ConfRev( 1 );
    //CB4.NdsCom( false );
    //CB4.DstAddress( dst_mac );

    // set the value of the undervoltage bit
    //CB4.dset.new_attribute(true);

    //if ( !mOutstation->schedule( CB4, 0x0003, 1 ) )
    //{
    //    std::cout << "Error: Failed to schedule dataset: "<< CB4.dset.name() << std::endl;
    //}

    if ( mInterface.empty() || mConfigurationFile.empty() )
    {
        std::cerr << "The interface and the data set subscription must be set before configuring the iec61850 outstation." << std::endl;
        return false;
    }

    try
    {
        // Configuration file will be GooseSub.cid.
        mOutstation.reset( new devices::outstation( mInterface.c_str() ) );
        mOutstation->configure( mConfigurationFile, std::bind( &bennu_61850::IEC61850RTU::process, this, std::placeholders::_1 ), 1, devices::time_unit_t::SECONDS );
        std::cout << "Sub num = " << mOutstation->subscriptions.size() << std::endl;
        for ( auto s : mOutstation->subscriptions )
        {
            std::cout << "Sub: name = " << s.second.name() << " ref = " << s.second.reference() << std::endl;

        }

        //ccss_protocols::iec61850::data_set data( mDataSetName );
        //data.add_entry( ccss_protocols::iec61850::Boolean::tag_value );
        //mOutstation->subscribe( data );
        //mOutstation->register_submodule( data.get_name(), this );

        //mOutstation->run();

    }
    catch (std::exception& e)
    {
        std::cerr << "iec61850 outstation failed to start due to the error: " << e.what() << "!" << std::endl;
        return false;
    }

    return true;
}


bool bennu_61850::IEC61850RTU::addRegister( const std::string& provider, const boost::uint16_t address, const sk::Device& device )
{
    size_t hash = registerDevice( device );
    mRegisters[address] = std::make_pair( hash, device.mField );


    return true;
}


void bennu_61850::IEC61850RTU::process( ccss_protocols::iec61850::goose::data_set& ds )
{

    if ( ds.num_entries() == 0 )
    {
        std::cerr << "Error: There is no data in the iec61850 data set!" << std::endl;
        std::cerr << "Data set name: " << ds.name() << "\nData set reference: " << ds.reference() << std::endl;
        logEvent( "read data store", "error", "There is no data in the iec61850 data set!" );
        return;
    }

    for ( size_t i = 0; i < ds.num_entries(); ++i )
    {
        std::ostringstream os;
        bool value = ds.get_attribute<protocols::Boolean>( i );
        if ( value && i == 0 )
        {
            if ( mHandlingRisingEdge )
            {
                std::cerr << "INFO: Already handling the rising edge of the TRIP." << std::endl;
                return;
            }
            else
            {
                std::cerr << "INFO: Begin handling the rising edge of a TRIP in the iec61850-rtu." << std::endl;
                auto timestamp = boost::posix_time::microsec_clock::local_time();
                std::cout << "time: " << timestamp.time_of_day() << std::endl;
            }

            mHandlingRisingEdge = true;

            // if the value being read is true, set the value as update for as long as it stays
            //  true. Once the value changes back to false, we won't set our value, but we'll allow
            //  the value to be changed on the subscriber.
            auto iter = mRegisters.find( i );
            if ( iter == mRegisters.end() )
            {
                //os << "Invalid register address: " << i;
                std::cerr << os.str() << std::endl;
                logEvent( "read data store", "error", os.str() );
                continue;
            }

            writeBoolData( iter->second.first, iter->second.second, !value );

            std::cout << "Sending an update for register address " << i << " controlling " << iter->second.first <<
                "'s field " << iter->second.second << std::endl;
            logEvent( "read data store", "info", "Value at register address 0 was received" );

        }
        else if ( value == false )
        {
            mHandlingRisingEdge = false;
        }
    }

}
