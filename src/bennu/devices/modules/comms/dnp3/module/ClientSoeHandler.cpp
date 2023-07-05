#include "ClientSoeHandler.hpp"

namespace bennu {
namespace comms {
namespace dnp3 {

ClientSoeHandler::ClientSoeHandler(std::weak_ptr<ClientConnection> rtuCon) :
    pRtuCon(rtuCon)
{
}

void ClientSoeHandler::Process(const HeaderInfo& info, const ICollection<Indexed<Binary>>& values)
{
    values.ForeachItem([&](const Indexed<Binary>& value)
    {
        pRtuCon.lock()->updateBinary(value.index, static_cast<bool>(value.value.value));
    });
}

void ClientSoeHandler::Process(const HeaderInfo& info, const ICollection<Indexed<Analog>>& values)
{
    values.ForeachItem([&](const Indexed<Analog>& value)
    {
        pRtuCon.lock()->updateAnalog(value.index, value.value.value);
    });
}

} // namespace dnp3
} // namespace comms
} // namespace bennu
