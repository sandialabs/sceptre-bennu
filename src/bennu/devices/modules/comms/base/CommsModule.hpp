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

    virtual ~CommsModule()
    {}

    // Stop any threads/servers the module owns and wait for them. Called once at process
    // exit (CommsModuleCreator::stopAll); must be idempotent. Default: nothing to stop.
    virtual void stop()
    {}

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
