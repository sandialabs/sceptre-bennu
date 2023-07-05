#ifndef BENNU_FIELDDEVICE_COMMS_DNP3_CLIENT_HPP
#define BENNU_FIELDDEVICE_COMMS_DNP3_CLIENT_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <boost/property_tree/ptree.hpp>

#include "opendnp3/DNP3Manager.h"

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/base/CommsClient.hpp"
#include "bennu/devices/modules/comms/dnp3/module/ClientConnection.hpp"
#include "bennu/utility/DirectLoggable.hpp"

namespace bennu {
namespace comms {
namespace dnp3 {

class ClientConnection;

using boost::property_tree::ptree;

class Client : public comms::CommsClient, public utility::DirectLoggable, public std::enable_shared_from_this<Client>
{
public:
    Client();

    ~Client();

    void addTagConnection(const std::string& tag, std::shared_ptr<ClientConnection> connection);
    void addTagConnection(const std::string& tag, std::shared_ptr<ClientConnection> connection, const bool sbo);

    bool tagRegister(const std::string& name, comms::RegisterType registerType, std::uint16_t address);

    std::shared_ptr<opendnp3::DNP3Manager> getManager()
    {
        return mManager;
    }

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
    std::shared_ptr<opendnp3::DNP3Manager> mManager; // DNP3 stack manager
    Client(const Client&);
    Client& operator =(const Client&);

    std::map<std::string, bool> mTagsForSBO;
};

} // namespace dnp3
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_DNP3_CLIENT_HPP
