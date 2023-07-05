#ifndef BENNU_FIELDDEVICE_IO_IOMODULE_HPP
#define BENNU_FIELDDEVICE_IO_IOMODULE_HPP

#include <string>

#include <boost/property_tree/ptree.hpp>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/distributed/Utils.hpp"

namespace bennu {
namespace io {

class IOModule
{
public:
    IOModule() = default;

    virtual ~IOModule() = default;

    virtual void start(const distributed::Endpoint& endpoint) = 0;

    void setDataManager(std::shared_ptr<field_device::DataManager> dm)
    {
        mDataManager = dm;
    }

    bool handleTreeData(const boost::property_tree::ptree& tree);

protected:
    std::shared_ptr<field_device::DataManager> mDataManager;

};

} // namespace io
} // namespace bennu

#endif //  BENNU_FIELDDEVICE_IO_IOMODULE_HPP
