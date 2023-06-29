#ifndef BENNU_FIELDDEVICE_COMMS_DNP3_CLIENTCONNECTION_HPP
#define BENNU_FIELDDEVICE_COMMS_DNP3_CLIENTCONNECTION_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opendnp3/DNP3Manager.h>

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/dnp3/module/Client.hpp"
#include "bennu/devices/modules/comms/dnp3/module/ClientSoeHandler.hpp"

namespace bennu {
namespace comms {
namespace dnp3 {

class Client;
class ClientSoeHandler;

class ClientConnection : public std::enable_shared_from_this<ClientConnection>
{
public:
    ClientConnection(std::weak_ptr<Client> client,
                     const std::uint16_t& address,
                     const std::string& rtuEndpoint,
                     const std::uint16_t& rtuAddress);

    void init();

    void start
    (
        const std::uint32_t scanRateAll,
        const std::uint32_t scanRateClass0,
        const std::uint32_t scanRateClass1,
        const std::uint32_t scanRateClass2,
        const std::uint32_t scanRateClass3
    );

    void addBinary(const std::string& tag, const comms::RegisterDescriptor& rd)
    {
        mRegisters[tag] = rd;
        mBinaryAddressToTagMapping[rd.mRegisterAddress] = tag;
    }

    void addAnalog(const std::string& tag, const comms::RegisterDescriptor& rd)
    {
        mRegisters[tag] = rd;
        mAnalogAddressToTagMapping[rd.mRegisterAddress] = tag;
    }

    void updateBinary(const std::uint16_t address, const bool status)
    {
        auto iter = mBinaryAddressToTagMapping.find(address);
        if (iter != mBinaryAddressToTagMapping.end())
        {
            mRegisters[iter->second].mStatus = status;
        }
    }

    void updateAnalog(const std::uint16_t address, const double value)
    {
        auto iter = mAnalogAddressToTagMapping.find(address);
        if (iter != mAnalogAddressToTagMapping.end())
        {
            mRegisters[iter->second].mFloatValue = value;
        }
    }

    bool getRegisterDescriptorByTag(const std::string& tag, comms::RegisterDescriptor& rd)
    {
        auto iter = mRegisters.find(tag);
        if (iter != mRegisters.end())
        {
            rd = iter->second;
            return true;
        }
        return false;
    }

    StatusMessage readRegisterByTag(const std::string& tag, comms::RegisterDescriptor& rd);

    StatusMessage selectBinary(const std::string& tag, bool status);

    StatusMessage writeBinary(const std::string& tag, bool status);

    StatusMessage selectAnalog(const std::string& tag, double value);

    StatusMessage writeAnalog(const std::string& tag, double value);

private:
    std::shared_ptr<ClientSoeHandler> pHandler;               // Pointer to SOE handler
    std::shared_ptr<opendnp3::DNP3Manager> mManager;          // Master stack manager
    std::shared_ptr<opendnp3::IChannel> mChannel;             // TCP client channel
    opendnp3::MasterStackConfig mStackConfig;                 // Master stack configuration
    std::shared_ptr<opendnp3::IMaster> mMaster;               // DNP3 master object
    std::uint16_t mAddress;                                   // DNP3 address of master
    std::string mRtuName;                                     // Name of remote RTU
    std::string mRtuEndpoint;                                 // IP/Port or DevName of remote RTU
    std::uint16_t mRtuAddress;                                // DNP3 address of remote RTU
    std::map<std::uint16_t, std::string> mBinaryAddressToTagMapping;
    std::map<std::uint16_t, std::string> mAnalogAddressToTagMapping;
    std::map<std::string, comms::RegisterDescriptor> mRegisters;

};

} // namespace dnp3
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_DNP3_CLIENTCONNECTION_HPP
