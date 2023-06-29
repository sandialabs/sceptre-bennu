#ifndef BENNU_FIELDDEVICE_BASE_FIELDDEVICE_HPP
#define BENNU_FIELDDEVICE_BASE_FIELDDEVICE_HPP

#include <memory>
#include <string>
#include <thread>

#include <boost/property_tree/ptree.hpp>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/devices/modules/io/InputModule.hpp"
#include "bennu/devices/modules/io/OutputModule.hpp"
#include "bennu/devices/modules/logic/LogicModule.hpp"
#include "bennu/utility/DirectLoggable.hpp"

namespace bennu {
namespace field_device {

using boost::property_tree::ptree;

class FieldDevice : public bennu::utility::DirectLoggable, public std::enable_shared_from_this<FieldDevice>
{
public:
    FieldDevice(const std::string& name);

    bool handleTreeData(const ptree& tree);

    std::shared_ptr<DataManager> getDataManager() const
    {
        return mDataManager;
    }

    void processOutputs();

    [[ noreturn ]] void scanCycle();

    void startDevice();

protected:
    std::string mFdName;
    std::shared_ptr<DataManager> mDataManager;
    std::shared_ptr<logic::LogicModule> mLogicModule;
    std::vector<std::shared_ptr<io::InputModule>> mInputModules;
    std::vector<std::shared_ptr<io::OutputModule>> mOutputModules;
    std::shared_ptr<std::thread> mScanThread;
    unsigned int mCycleTime;

private:
    FieldDevice(const FieldDevice&);
    FieldDevice& operator =(const FieldDevice&);

};

} // namespace field_device
} // namespace bennu

#endif // BENNU_FIELDDEVICE_BASE_FIELDDEVICE_HPP
