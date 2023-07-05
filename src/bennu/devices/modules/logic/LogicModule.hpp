#ifndef BENNU_FIELDDEVICE_LOGIC_LOGICMODULE_HPP
#define BENNU_FIELDDEVICE_LOGIC_LOGICMODULE_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/property_tree/ptree.hpp>

#include "bennu/devices/field-device/DataManager.hpp"

namespace bennu {
namespace logic {

class LogicModule : public std::enable_shared_from_this<LogicModule>
{
public:
    LogicModule();

    void setDataManager(std::shared_ptr<field_device::DataManager> dm)
    {
        mDataManager = dm;
    }

    bool handleTreeData(const boost::property_tree::ptree& tree);

    void scanInputs();

    void scanLogic(unsigned int cycleTime);

    bool isDelayedTag(std::string& tag)
    {
        return mDelayedTags.count(tag);
    }

    void setDelayedTag(std::string& tag, int delayMillis)
    {
        mDelayedTags[tag] = delayMillis;
    }

    int getDelayedTag(std::string& tag)
    {
        return mDelayedTags[tag];
    }

    void removeDelayedTag(std::string& tag)
    {
        mDelayedTags.erase(tag);
    }

private:
    void mReplaceAllRhs(std::string & data, std::string toSearch, std::string replaceStr);
    std::vector<std::string> mSplitExpression(std::string phrase, std::string delimiter);
    std::vector<std::string> mSplitStr(std::string phrase, std::string delimiter);
    std::vector<std::string> mSortByLargest(std::vector<std::string> vector);
    int mGetDelay(std::vector<std::string> rhsParts);
    std::map<std::string, int> mDelayedTags;
    std::string mLogic;
    std::string mCurrentLogic;
    std::shared_ptr<field_device::DataManager> mDataManager;

};

} // namespace logic
} // bennu

#endif // BENNU_FIELDDEVICE_LOGIC_LOGICMODULE_HPP
