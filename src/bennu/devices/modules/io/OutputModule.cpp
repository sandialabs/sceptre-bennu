#include "OutputModule.hpp"

#include "bennu/utility/Log.hpp"

namespace bennu {
namespace io {

OutputModule::OutputModule() :
    IOModule()
{
}

void OutputModule::start(const distributed::Endpoint& endpoint)
{
    mClient.reset(new distributed::Client(endpoint));
    setSender([this](const std::string& p) { return mClient->writePoints(p); },
              [this]() { return mClient->replied(); });
}

void OutputModule::scanOutputs()
{
    // snapshot under lock; tags without a point (internal) stay in the map for updateInternalData
    std::vector<std::pair<std::string, bool>> bins;
    std::vector<std::pair<std::string, double>> anals;
    std::vector<std::string> binTags, analTags;
    std::string point;
    for (auto& t : mDataManager->snapshotUpdatedBinaryTags())
    {
        if (mDataManager->getPointByTag(t.first, point) && sends(point))
        {
            bins.emplace_back(point, t.second);
            binTags.push_back(t.first);
        }
    }
    for (auto& t : mDataManager->snapshotUpdatedAnalogTags())
    {
        if (mDataManager->getPointByTag(t.first, point) && sends(point))
        {
            anals.emplace_back(point, t.second);
            analTags.push_back(t.first);
        }
    }
    if (bins.empty() && anals.empty())
    {
        return;
    }

    if (!mSend(distributed::Client::buildWritePayload(bins, anals)))
    {
        // No reply at all = outage: leave maps untouched and retry next cycle (don't fan out, each
        // send can block ~15 s). An ERR reply can reject a whole batch for one bad point, so retry
        // per point; points the provider rejects are dropped, as before batching.
        bool outage = !mReplied();
        std::vector<std::string> rejected;
        if (!outage && bins.size() + anals.size() == 1)
        {
            rejected.push_back(bins.empty() ? anals[0].first : bins[0].first);
        }
        else if (!outage)
        {
            auto tryOne = [&](const std::string& p, const std::string& payload) {
                if (mSend(payload))
                {
                    return;
                }
                rejected.push_back(p);
                outage = !mReplied();
            };
            for (auto it = bins.begin(); !outage && it != bins.end(); ++it)
            {
                tryOne(it->first, distributed::Client::buildWritePayload({*it}, {}));
            }
            for (auto it = anals.begin(); !outage && it != anals.end(); ++it)
            {
                tryOne(it->first, distributed::Client::buildWritePayload({}, {*it}));
            }
        }
        if (outage)
        {
            if (!mWriteFailed)
            {
                BENNU_LOG_ERROR("E: OutputModule: provider not responding, will retry writes\n");
                mWriteFailed = true;
            }
            return;
        }
        for (auto& r : rejected)
        {
            BENNU_LOG_ERROR("E: OutputModule: write of %s rejected, dropping\n", r.c_str());
        }
    }
    mWriteFailed = false;

    for (size_t i = 0; i < bins.size(); ++i)
    {
        mDataManager->setDataByTag<bool>(binTags[i], bins[i].second);
        mDataManager->deleteBinaryTag(binTags[i], bins[i].second);
    }
    for (size_t i = 0; i < anals.size(); ++i)
    {
        mDataManager->setDataByTag<double>(analTags[i], anals[i].second);
        mDataManager->deleteAnalogTag(analTags[i], anals[i].second);
    }
}

} // namespace io
} // namespace bennu
