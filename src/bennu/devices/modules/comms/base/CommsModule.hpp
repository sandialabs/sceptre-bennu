#ifndef BENNU_FIELDDEVICE_COMMS_COMMSMODULE_HPP
#define BENNU_FIELDDEVICE_COMMS_COMMSMODULE_HPP

#include <memory>

#include "bennu/devices/field-device/DataManager.hpp"

namespace bennu {
namespace comms {

class CommsModule
{
public:
    CommsModule() {}

    ~CommsModule() {}

    void setDataManager(std::shared_ptr<field_device::DataManager> dm)
    {
        mDataManager = dm;
    }

protected:
    std::shared_ptr<field_device::DataManager> mDataManager;

};

} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_COMMSMODULE_HPP
