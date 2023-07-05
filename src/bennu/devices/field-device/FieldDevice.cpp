#include "FieldDevice.hpp"

#include <iostream>

#include "bennu/devices/modules/comms/base/CommsModuleCreator.hpp"

namespace bennu {
namespace field_device {

using boost::property_tree::ptree_bad_path;
using boost::property_tree::ptree_error;

FieldDevice::FieldDevice(const std::string& name) :
    bennu::utility::DirectLoggable(name),
    mFdName(name),
    mDataManager(new DataManager)
{
}

bool FieldDevice::handleTreeData(const ptree& tree)
{
    try
    {
        mCycleTime = tree.get<unsigned int>("cycle-time", 1000);

        if (tree.get_child_optional("logic"))
        {
            auto logicModule{new logic::LogicModule};
            logicModule->setDataManager(mDataManager);
            logicModule->handleTreeData(tree);
            mLogicModule.reset(logicModule);
        }

        auto inputs = tree.equal_range("input");
        for (auto iter = inputs.first; iter != inputs.second; ++iter)
        {
            std::shared_ptr<io::InputModule> mod {new io::InputModule()};
            mod->setDataManager(mDataManager);
            mod->handleTreeData(iter->second);
            mInputModules.push_back(mod);
        }

        auto outputs = tree.equal_range("output");
        for (auto iter = outputs.first; iter != outputs.second; ++iter)
        {
            std::shared_ptr<io::OutputModule> mod {new io::OutputModule()};
            mod->setDataManager(mDataManager);
            mod->handleTreeData(iter->second);
            mOutputModules.push_back(mod);
        }

        if (tree.get_child_optional("tags"))
        {
            ptree tagTree = tree.get_child("tags");
            auto extTags = tagTree.equal_range("external-tag");
            for (auto extIter = extTags.first; extIter != extTags.second; ++extIter)
            {
                std::string tag = extIter->second.get<std::string>("name");
                std::string reg = extIter->second.get<std::string>("io");
                std::string type = extIter->second.get<std::string>("type");
                if (!mDataManager->addTagToPointMapping(tag, reg))
                {
                    std::cout << "ERROR: Cannot map an alias for tag " << tag << " to register " << reg << std::endl;
                }
                else
                {
                    if (type == "binary")
                    {
                        mDataManager->addBinaryTag(tag);
                    }
                    else if (type == "analog")
                    {
                        mDataManager->addAnalogTag(tag);
                    }
                }
            }

            auto intTags = tagTree.equal_range("internal-tag");
            for (auto intIter = intTags.first; intIter != intTags.second; ++intIter)
            {
                std::string tag = intIter->second.get<std::string>("name");
                if (intIter->second.get_child_optional("status"))
                {
                    mDataManager->addInternalData(tag, intIter->second.get<bool>("status"));
                    mDataManager->addBinaryTag(tag);
                }
                else if (intIter->second.get_child_optional("value"))
                {
                    mDataManager->addInternalData(tag, intIter->second.get<double>("value"));
                    mDataManager->addAnalogTag(tag);
                }
            }
        }

        if (tree.get_child_optional("comms"))
        {
            ptree commsTree = tree.get_child("comms");
            comms::CommsModuleCreator::the()->handleCommsTreeData(commsTree, mDataManager);
        }

        startDevice();
        return true;
    }
    catch (ptree_bad_path& e)
    {
        std::cerr << "ERROR: Invalid xml in field device tags: " << e.what() << std::endl;
        return false;
    }
    catch (ptree_error& e)
    {
        std::cerr << "ERROR: There was a problem parsing field device tags: " << e.what() << std::endl;
        return false;
    }
}

void FieldDevice::processOutputs()
{
    for (auto& mod : mOutputModules)
    {
        mod->scanOutputs();
    }
    mDataManager->updateInternalData();
    mDataManager->clearUpdatedTags();
}

void FieldDevice::startDevice()
{
    mScanThread.reset(new std::thread(std::bind(&FieldDevice::scanCycle, this)));
}

void FieldDevice::scanCycle()
{
    int i = 1;
    while (1)
    {
        if (mLogicModule)
        {
            mLogicModule->scanInputs();
            mLogicModule->scanLogic(mCycleTime);
        }
        processOutputs();
        if (i % 10 == 0)
        {
            mDataManager->printExternalData();
            i = 1;
        }
        else
        {
            i++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(mCycleTime));
    }
}

} // namespace field_device
} // namespace bennu
