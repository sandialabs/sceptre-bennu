#ifndef BENNU_FIELDDEVICE_COMMS_BACNET_DATAHANDLER_HPP
#define BENNU_FIELDDEVICE_COMMS_BACNET_DATAHANDLER_HPP

#include <cstdint>
#include <memory>

#include <boost/property_tree/ptree.hpp>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/comms/base/CommsModule.hpp"
#include "bennu/devices/modules/comms/bacnet/module/Client.hpp"
#include "bennu/devices/modules/comms/bacnet/module/Server.hpp"

namespace bennu {
namespace comms {
namespace bacnet {

using boost::property_tree::ptree;

class DataHandler
{
public:
    std::shared_ptr<CommsModule> handleServerTreeData(const ptree& tree, std::shared_ptr<field_device::DataManager> dm);
    std::shared_ptr<CommsModule> handleClientTreeData(const ptree& tree, std::shared_ptr<field_device::DataManager> dm);

protected:
    void parseServerTree(std::shared_ptr<Server> server, const ptree& tree);

    void parseClientTree(std::shared_ptr<Client> client, const ptree& tree);

};

} // namespace bacnet
} // namespace comms
} // namespace bennu

// These declarations are needed both here and in DataHandler.cpp because of the C-Code interactions
extern std::map<uint32_t, std::shared_ptr<bennu::comms::bacnet::Server>> instanceToServerMap;
extern std::map<uint32_t, std::shared_ptr<bennu::comms::bacnet::ClientConnection>> instanceToClientConnectionMap;

#endif // BENNU_FIELDDEVICE_COMMS_BACNET_DATAHANDLER_HPP
