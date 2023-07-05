#ifndef BENNU_FIELDDEVICE_COMMS_DNP3_DATAHANDLER_HPP
#define BENNU_FIELDDEVICE_COMMS_DNP3_DATAHANDLER_HPP

#include <memory>

#include <boost/property_tree/ptree.hpp>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/comms/base/CommsModule.hpp"
#include "bennu/devices/modules/comms/dnp3/module/Client.hpp"
#include "bennu/devices/modules/comms/dnp3/module/Server.hpp"

namespace bennu {
namespace comms {
namespace dnp3 {

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

} // namespace dnp3
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_DNP3_DATAHANDLER_HPP
