#ifndef BENNU_FIELDDEVICE_COMMS_BACNET_CLIENTCONNECTION_HPP
#define BENNU_FIELDDEVICE_COMMS_BACNET_CLIENTCONNECTION_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "bennu/devices/modules/comms/base/Common.hpp"

namespace bennu {
namespace comms {
namespace bacnet {

class ClientConnection : public std::enable_shared_from_this<ClientConnection>
{
public:
    ClientConnection(int instance,
                     const std::string& rtuEndpoint,
                     const std::uint32_t& rtuInstance,
                     std::uint32_t scanRate);

    ~ClientConnection();

    void start();

    void poll();

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

    StatusMessage writeBinary(const std::string& tag, bool status);

    StatusMessage writeAnalog(const std::string& tag, double value);

private:
    int mInstance;                              // BACnet client instance #
    std::string mRtuEndpoint;                   // IP/Port or DevName of remote RTU
    std::uint16_t mRtuInstance;                 // BACnet instance of remote RTU
    std::uint16_t mScanRate;                    // Polling scan rate
    std::shared_ptr<std::thread> pPollThread;   // Client polling thread
    std::map<std::uint16_t, std::string> mBinaryAddressToTagMapping;
    std::map<std::uint16_t, std::string> mAnalogAddressToTagMapping;
    std::map<std::string, comms::RegisterDescriptor> mRegisters;

};

} // namespace bacnet
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_BACNET_CLIENTCONNECTION_HPP
