#ifndef BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CLIENTCONNECTION_HPP
#define BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CLIENTCONNECTION_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/error-codes.hpp"
#include "bennu/devices/modules/comms/modbus/protocol/protocol-stack.hpp"
#include "bennu/distributed/AbstractClient.hpp"
#include "bennu/distributed/TcpClient.hpp"
#include "bennu/distributed/SerialClient.hpp"

namespace bennu {
namespace comms {
namespace modbus {

// This holds the message that will be sent to the Rtu
//  and the registers that are part of this call.
struct ConnectionMessage
{
    comms::RegisterType mRegisterType;
    std::set<comms::RegisterDescriptor, comms::RegDescComp> mRegisters;
};

class ClientConnection
{
public:
    struct ScaledValue
    {
        std::pair<double, double> mRange;
        double mSlope;
        double mIntercept;
    };

    ClientConnection(const std::string& endpoint, uint8_t unitId);

    std::string logError(error_code_t::type error, const std::string& type, std::uint16_t startAddress, size_t size);

    void addRegister(const std::string& tag, const comms::RegisterDescriptor& rd)
    {
        mRegisters[tag] = rd;
    }

    bool getRegisterAddressesByType(comms::RegisterType registerType, std::set<std::uint16_t>& addresses) const
    {
        addresses.clear();
        for (auto& reg : mRegisters)
        {
            if (reg.second.mRegisterType == registerType)
            {
                addresses.insert(reg.second.mRegisterAddress);
            }
        }

        if (addresses.empty())
        {
            return false;
        }

        return true;
    }

    bool getRegisterDescriptorsByType(comms::RegisterType registerType, std::set<comms::RegisterDescriptor, comms::RegDescComp>& registers) const
    {
        registers.clear();
        for (auto& reg : mRegisters)
        {
            if (reg.second.mRegisterType == registerType)
            {
                registers.insert(reg.second);
            }
        }

        if (registers.empty())
        {
            return false;
        }

        return true;
    }

    bool getRegisterDescriptorByAddress(size_t address, comms::RegisterDescriptor& regDesc) const
    {
        for (auto& reg : mRegisters)
        {
            if (reg.second.mRegisterAddress == address)
            {
                regDesc = reg.second;
                return true;
            }
        }

        return false;
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

    void getRegisterTags(std::set<std::string>& tags) const
    {
        for (auto& tag : mRegisters)
        {
            tags.insert(tag.first);
        }
    }

    const std::string& getName() const
    {
        return mName;
    }

    void setName(const std::string& name)
    {
        mName = name;
    }

    void setRange(uint16_t address, const std::pair<double, double>& range)
    {
        ScaledValue sv;
        sv.mRange = range;
        sv.mSlope = 65535.0 / (sv.mRange.first - sv.mRange.second);
        sv.mIntercept = -(sv.mSlope * sv.mRange.second);
        mScaledValues[address] = sv;
    }

    const std::pair<double, double>& getRange(const std::pair<double, double>& range) const
    {
        return mRange;
    }

    StatusMessage readRegisterByTag(const std::string& tag, comms::RegisterDescriptor& rd);
    StatusMessage readRegisters(const std::vector<ConnectionMessage>& messages, std::vector<comms::RegisterDescriptor>& responses, std::vector<comms::LogMessage>& logMessages);

    StatusMessage writeCoil(const std::string& tag, bool value);
    StatusMessage writeHoldingRegister(const std::string& tag, float value);

    const std::vector<ConnectionMessage>& getCurrentResponses() const
    {
        return mResponses;
    }

    void readHandler(const boost::system::error_code& error, size_t bytesTransferred);

private:
    std::string mName;
    std::pair<double, double> mRange;
    double mSlope;
    double mIntercept;
    bool mPersistConnection;
    std::shared_ptr<protocol_stack> mProtocolStack;
    std::map<std::string, comms::RegisterDescriptor> mRegisters;
    std::map<uint16_t, ScaledValue> mScaledValues;
    std::vector<ConnectionMessage> mResponses;
    std::shared_ptr<utility::AbstractClient> mClient;

};

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CLIENTCONNECTION_HPP
