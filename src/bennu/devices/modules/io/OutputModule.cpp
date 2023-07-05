#include "OutputModule.hpp"

namespace bennu {
namespace io {

OutputModule::OutputModule() :
    IOModule()
{
}

void OutputModule::start(const distributed::Endpoint& endpoint)
{
    mClient.reset(new distributed::Client(endpoint));
}

void OutputModule::scanOutputs()
{
    auto bTags = mDataManager->getUpdatedBinaryTags();
    for (auto& t : bTags)
    {
        std::string point;
        if (mDataManager->getPointByTag(t.first, point))
        {
            // write to provider
            mClient->writePoint(point, t.second);
            // write to rtu datastore
            mDataManager->setDataByTag<bool>(t.first, t.second);
        }
    }
    auto aTags = mDataManager->getUpdatedAnalogTags();
    for (auto& t : aTags)
    {
        std::string point;
        if (mDataManager->getPointByTag(t.first, point))
        {
            // write to provider
            mClient->writePoint(point, t.second);
            // write to rtu datastore
            mDataManager->setDataByTag<double>(t.first, t.second);
        }
    }
}

} // namespace io
} // namespace bennu
