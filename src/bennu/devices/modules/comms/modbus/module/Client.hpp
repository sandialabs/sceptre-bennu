#ifndef BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CLIENT_HPP
#define BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CLIENT_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <boost/asio.hpp>

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/base/CommsClient.hpp"
#include "bennu/devices/modules/comms/modbus/module/ClientConnection.hpp"
#include "bennu/utility/DirectLoggable.hpp"

namespace bennu {
namespace comms {
namespace modbus {

class Client : public comms::CommsClient, public utility::DirectLoggable, public std::enable_shared_from_this<Client>
{
public:
    Client();

    ~Client();

    void addTagConnection(const std::string& tag, std::shared_ptr<ClientConnection> connection);

    bool tagRegister(const std::string& name, comms::RegisterType registerType, std::uint16_t address);

    const std::map<std::string, std::shared_ptr<ClientConnection>>& getConnections() const
    {
        return mTagsToConnection;
    }

    virtual std::set<std::string> getTags() const;
    virtual bool isValidTag(const std::string& tag) const;
    virtual StatusMessage readTag(const std::string& tag, comms::RegisterDescriptor& rd) const;
    virtual StatusMessage writeBinaryTag(const std::string& tag, bool status);
    virtual StatusMessage writeAnalogTag(const std::string& tag, double value);

private:
    std::map<std::string, std::shared_ptr<ClientConnection>> mTagsToConnection;
    std::timed_mutex mLock;
    Client(const Client&);
    Client& operator =(const Client&);

};

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_CLIENT_HPP
