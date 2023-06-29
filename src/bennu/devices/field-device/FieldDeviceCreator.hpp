#ifndef BENNU_FIELDDEVICE_BASE_FIELDDEVICEMANAGER_HPP
#define BENNU_FIELDDEVICE_BASE_FIELDDEVICEMANAGER_HPP

#include <memory>
#include <vector>

#include <boost/property_tree/ptree.hpp>

#include "bennu/utility/Singleton.hpp"

namespace bennu {
namespace field_device {

class FieldDevice;
class FieldDeviceDataHandler;

using boost::property_tree::ptree;

class FieldDeviceCreator : public utility::Singleton<FieldDeviceCreator>
{
public:
    friend class utility::Singleton<FieldDeviceCreator>;

    bool handleTreeData(const ptree& tree);

protected:
    std::shared_ptr<bennu::field_device::FieldDevice> mFieldDevice;

    FieldDeviceCreator() {}

    FieldDeviceCreator(const FieldDeviceCreator&);

    FieldDeviceCreator& operator =(const FieldDeviceCreator&);

};

} // namespace field_device
} // namespace bennu

#endif // BENNU_FIELDDEVICE_BASE_FIELDDEVICEMANAGER_HPP
