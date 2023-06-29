#ifndef BENNU_FIELDDEVICE_COMMS_DNP3_CLIENTSOEHANDLER_HPP
#define BENNU_FIELDDEVICE_COMMS_DNP3_CLIENTSOEHANDLER_HPP

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "opendnp3/master/ISOEHandler.h"
#include "opendnp3/master/ResponseInfo.h"

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/dnp3/module/Client.hpp"
#include "bennu/devices/modules/comms/dnp3/module/ClientConnection.hpp"

namespace bennu {
namespace comms {
namespace dnp3 {

class Client;
class ClientConnection;

using namespace opendnp3;

class ClientSoeHandler : public ISOEHandler
{
public:
    ClientSoeHandler(std::weak_ptr<ClientConnection> rtuCon);

    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<Binary>>& values) override;
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<DoubleBitBinary>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<Analog>>& values) override;
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<Counter>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<FrozenCounter>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<BinaryOutputStatus>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<AnalogOutputStatus>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<OctetString>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<TimeAndInterval>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<BinaryCommandEvent>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<Indexed<AnalogCommandEvent>>& values) override {}
    virtual void Process(const HeaderInfo& info, const ICollection<DNPTime>& values) override {}

    virtual void BeginFragment(const ResponseInfo& info) final {}
    virtual void EndFragment(const ResponseInfo& info) final {}

private:
    std::weak_ptr<ClientConnection> pRtuCon;

    void resetTypes(std::multimap<comms::RegisterType, comms::RegisterDescriptor>& rMap)
    {
        for (auto iter = rMap.begin(); iter != rMap.end(); ++iter)
        {
            iter->second.mRegisterType = comms::eNone;
        }
    }

    std::vector<comms::RegisterDescriptor> registersFromMap(std::multimap<comms::RegisterType, comms::RegisterDescriptor> map)
    {
        std::vector<comms::RegisterDescriptor> rds;
        for (auto iter = map.begin(); iter != map.end(); ++iter)
        {
            rds.push_back(iter->second);
        }
        return rds;
    }

    template <class T>
    static void PrintAll(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<T>>& values)
    {
        auto print = [&](const opendnp3::Indexed<T>& pair)
        {
            Print<T>(info, pair.value, pair.index);
        };
        values.ForeachItem(print);
    }

    template <class T>
    static void Print(const opendnp3::HeaderInfo& info, const T& value, std::uint16_t index)
    {
        std::cout << "[" << index << "] : " <<
                  ValueToString(value) << " : " <<
                  static_cast<int>(value.flags.value) << " : " <<
                  value.time << std::endl;
    }

    template <class T>
    static std::string ValueToString(const T& meas)
    {
        std::ostringstream oss;
        oss << meas.value;
        return oss.str();
    }
};

} // namespace dnp3
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_DNP3_CLIENTSOEHANDLER_HPP
