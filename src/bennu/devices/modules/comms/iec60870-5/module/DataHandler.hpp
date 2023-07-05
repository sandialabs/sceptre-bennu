#ifndef BENNU_FIELDDEVICE_COMMS_IEC60870_5_DATAHANDLER_HPP
#define BENNU_FIELDDEVICE_COMMS_IEC60870_5_DATAHANDLER_HPP

#include <memory>
#include <string>

#include <boost/property_tree/ptree.hpp>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/comms/base/CommsModule.hpp"
#include "bennu/devices/modules/comms/iec60870-5/module/Client.hpp"
#include "bennu/devices/modules/comms/iec60870-5/module/Server.hpp"

namespace bennu {
namespace comms {
namespace iec60870 {

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

} // namespace iec60870
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_IEC60870_5_DATAHANDLER_HPP
