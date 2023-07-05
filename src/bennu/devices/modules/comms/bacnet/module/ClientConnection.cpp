#include "ClientConnection.hpp"

#include <iostream>
#include <functional>

#include "bacnet/bacdef.h"
#include "bacnet/bacenum.h"
#include "bacnet/basic/object/bacnet.h"
#include "ports/linux/bacport.h"

namespace bennu {
namespace comms {
namespace bacnet {

ClientConnection::ClientConnection(int instance,
                                   const std::string& rtuEndpoint,
                                   const std::uint32_t& rtuInstance,
                                   std::uint32_t scanRate) :
    mInstance(instance),
    mRtuEndpoint(rtuEndpoint),
    mRtuInstance(rtuInstance),
    mScanRate(scanRate)
{
}

ClientConnection::~ClientConnection()
{
    // Delete address cache file
    std::remove("address_cache");
}

StatusMessage ClientConnection::readRegisterByTag(const std::string& tag, comms::RegisterDescriptor& rd)
{
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    StatusMessage sm = STATUS_INIT;
    sm.status = status;
    if (!status)
    {
        auto msg = "readRegisterByTag(): Unable to find tag -- " + tag;
        sm.message = msg.data();
    }
    return sm;
}

StatusMessage ClientConnection::writeBinary(const std::string& tag, bool value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        auto msg = "writeBinary(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = msg.data();
        return sm;
    }
    auto type = rd.mRegisterType == RegisterType::eValueReadOnly ? OBJECT_BINARY_INPUT : OBJECT_BINARY_OUTPUT;
    auto val = value ? BINARY_ACTIVE : BINARY_INACTIVE;
    // Write boolean using protocol
    StatusMessage result =
            BacnetWriteProperty(mRtuInstance,           // device instance
                                type,                   // object type
                                rd.mRegisterAddress,    // object instance
                                PROP_PRESENT_VALUE,     // object property
                                BACNET_NO_PRIORITY,     // object priority
                                BACNET_ARRAY_ALL,       // object index
                                std::to_string(BACNET_APPLICATION_TAG_ENUMERATED).data(),   // tag
                                std::to_string(val).data());      // value
    if (!result.status)
    {
        return result;
    }
    // Update local data so we don't have to wait until the next poll
    updateBinary(rd.mRegisterAddress, status);
    return sm;
}

StatusMessage ClientConnection::writeAnalog(const std::string& tag, double value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        auto msg = "writeAnalog(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = msg.data();
        return sm;
    }
    auto type = rd.mRegisterType == RegisterType::eValueReadOnly ? OBJECT_ANALOG_INPUT : OBJECT_ANALOG_OUTPUT;
    // Write analog using protocol. Returns 1 if failed
    auto result = BacnetWriteProperty(mRtuInstance,           // device instance
                                      type,                   // object type
                                      rd.mRegisterAddress,    // object instance
                                      PROP_PRESENT_VALUE,     // object property
                                      BACNET_NO_PRIORITY,     // object priority
                                      BACNET_ARRAY_ALL,       // object index
                                      std::to_string(BACNET_APPLICATION_TAG_REAL).data(),   // tag
                                      std::to_string(value).data());    // value
    if (!result.status)
    {
        return result;
    }
    // Update local data so we don't have to wait until the next poll
    updateAnalog(rd.mRegisterAddress, value);
    return sm;
}

void ClientConnection::start()
{
    // If endpoint starts with udp://, parse ip/port and use UDP
    std::size_t findResult = mRtuEndpoint.find("udp://");

    if (findResult != std::string::npos)
    {
        // Handle splitting out ip and port from endpoint
        std::string ipAndPort = mRtuEndpoint.substr(findResult + 6);
        // Set up initial client comms
        BacnetPrepareClientComm(mInstance, mRtuInstance, ipAndPort.data());
        // Try to bind to device
        bool failed = BacnetBindToDevice(mRtuInstance);
        if (failed)
        {
            std::cout << "Error -- Could not start BACnet-CLIENT -- RTU Connection: " << mRtuEndpoint << " (" << mRtuInstance << ") " << std::endl;
            std::cout << "Exiting." << std::endl;
            exit(-1);
        }
        else
        {
            // Start polling
            pPollThread.reset(new std::thread(std::bind(&ClientConnection::poll, this)));
            std::cout << "Started BACnet-CLIENT (" << mInstance << ") -- RTU Connection: " << mRtuEndpoint << " (" << mRtuInstance << ") " << std::endl;
        }
    }
    else
    {
        std::cout << "Error -- Unknown endpoint protocol: " << mRtuEndpoint << std::endl;
        exit(-1);
    }
}

void ClientConnection::poll()
{
    BACNET_OBJECT_TYPE type;
    comms::RegisterDescriptor rd;
    while (1)
    {
        // Poll our binary points
        // kv = {<reg address>: <tag>}
        for (const auto& kv : mBinaryAddressToTagMapping)
        {
            getRegisterDescriptorByTag(kv.second, rd);
            type = rd.mRegisterType == RegisterType::eValueReadOnly ? OBJECT_BINARY_INPUT : OBJECT_BINARY_OUTPUT;
            StatusMessage sm = BacnetReadProperty(mRtuInstance,        // device instance
                                                  type,                // object type
                                                  kv.first,            // object instance
                                                  PROP_PRESENT_VALUE,  // object property
                                                  -1);                 // object index (-1 is ignored)
            if (!sm.status)
            {
                std::cout << "Error sending BAcnetReadProperty -- " << sm.message << std::endl;
            }
        }
        // Poll our analog points
        for (const auto& kv : mAnalogAddressToTagMapping)
        {
            getRegisterDescriptorByTag(kv.second, rd);
            type = rd.mRegisterType == RegisterType::eValueReadOnly ? OBJECT_ANALOG_INPUT : OBJECT_ANALOG_OUTPUT;
            StatusMessage sm = BacnetReadProperty(mRtuInstance,        // device instance
                                                  type,                // object type
                                                  kv.first,            // object instance
                                                  PROP_PRESENT_VALUE,  // object property
                                                  -1);                 // object index (-1 is ignored)
            if (!sm.status)
            {
                std::cout << "Error sending BAcnetReadProperty -- " << sm.message << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(mScanRate));
    }
}

} // namespace bacnet
} // namespace comms
} // namespace bennu
