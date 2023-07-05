#ifndef BENNU_FIELDDEVICE_COMMS_COMMSMODULECREATOR_HPP
#define BENNU_FIELDDEVICE_COMMS_COMMSMODULECREATOR_HPP

#include <memory>
#include <vector>

#include <boost/property_tree/ptree.hpp>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/utility/Singleton.hpp"

namespace bennu {
namespace field_device
{
    class DataManager;
}
namespace comms {

class CommsModule;

typedef std::function<std::shared_ptr<CommsModule> (const boost::property_tree::ptree& tree,
    std::shared_ptr<field_device::DataManager> dm)> CommsDataHandler;

class CommsModuleCreator : public utility::Singleton<CommsModuleCreator>
{
public:
    friend class utility::Singleton<CommsModuleCreator>;

    const std::vector<std::shared_ptr<CommsModule>>& getCommsModules() const
    {
        return mCommsModules;
    }

    void addCommsDataHandler(CommsDataHandler handler)
    {
        mCommsDataHandlers.push_back(handler);
    }

    void handleCommsTreeData(const boost::property_tree::ptree& tree, std::shared_ptr<field_device::DataManager> dm);

protected:
    std::vector<CommsDataHandler> mCommsDataHandlers;

    std::vector<std::shared_ptr<CommsModule>> mCommsModules;

    CommsModuleCreator() {}

    virtual ~CommsModuleCreator() {}

    CommsModuleCreator(const CommsModuleCreator&);

    CommsModuleCreator& operator =(const CommsModuleCreator&);

};

} // namespace field_device
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_COMMSMODULECREATOR_HPP

