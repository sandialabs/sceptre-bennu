#ifndef BENNU_FIELDDEVICE_DATAMANAGER_HPP
#define BENNU_FIELDDEVICE_DATAMANAGER_HPP

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex> // std::scoped_lock
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "bennu/devices/field-device/DataStore.hpp"

namespace bennu {
namespace field_device {

class DataManager
{
public:
    DataManager() :
        mInternalData(new DataStore<std::string>), // tags
        mExternalData(new DataStore<std::string>)  // i/o points
    {
    }

    ~DataManager() = default;

    // add Binary/Analog data point from Input/Output modules
    template <typename T>
    void addExternalData(const std::string& id, const std::string& point)
    {
        mExternalData->addData(point, T());
        mExternalPoints[id] = point;
    }

    // add 'internal-tag' data point
    template <typename T>
    void addInternalData(const std::string& tag, const T& value)
    {
        mInternalData->addData(tag, value);
    }

    bool getPointByTag(const std::string& tag, std::string& point) const
    {
        auto iter = mTagToPoint.find(tag);
        if (iter != mTagToPoint.end())
        {
            auto pointIter = mExternalPoints.find(iter->second);
            if (pointIter != mExternalPoints.end())
            {
                point = pointIter->second;
                return true;
            }
        }
        return false;
    }

    template <typename T>
    T getDataByTag(const std::string& tag) const
    {
        auto iter = mTagToPoint.find(tag);
        if (iter != mTagToPoint.end())
        {
            auto pointIter = mExternalPoints.find(iter->second);
            if (pointIter != mExternalPoints.end())
            {
                return mExternalData->getData<T>(pointIter->second);
            }
        }
        else
        {
            return mInternalData->getData<T>(tag);
        }
        return T();
    }

    double getTimestampByTag(const std::string& tag) const
    {
        auto iter = mTagToPoint.find(tag);
        if (iter != mTagToPoint.end())
        {
            auto pointIter = mExternalPoints.find(iter->second);
            if (pointIter != mExternalPoints.end())
            {
                return mExternalData->getTimestamp(pointIter->second);
            }
        }

        return 0;
    }

    template <typename T>
    bool setDataByTag(const std::string& tag, const T& value) const
    {
        auto iter = mTagToPoint.find(tag);
        if (iter != mTagToPoint.end())
        {
            auto pointIter = mExternalPoints.find(iter->second);
            if (pointIter != mExternalPoints.end())
            {
                return mExternalData->setData<T>(pointIter->second, value);
            }
            return false;
        }
        return false;
    }

    template <typename T>
    bool setDataByPoint(const std::string& point, const T& value) const
    {
        double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        return mExternalData->setData<T>(point, value, ts);
    }

    bool addTagToPointMapping(const std::string& tag, const std::string& point)
    {
        if (mExternalPoints.find(point) != mExternalPoints.end())
        {
            mTagToPoint[tag] = point;
            return true;
        }
        return false;
    }

    bool hasTag(const std::string& tag) const
    {
        auto iter = mTagToPoint.find(tag);
        if (iter != mTagToPoint.end())
        {
            auto extIter = mExternalPoints.find(iter->second);
            if (extIter != mExternalPoints.end())
            {
                return mExternalData->hasData(extIter->second);
            }
            return false;
        }
        return (mInternalData->hasData(tag));
    }

    bool hasPoint(const std::string& point) const
    {
        return mExternalData->hasData(point);
    }

    std::shared_ptr<DataStore<std::string>> getExternalData() const
    {
        return mExternalData;
    }

    void printExternalData()
    {
        mExternalData->printData();
    }

    // points owned by some OutputModule; set at config load, before any threads run
    void addOutputPoint(const std::string& point)
    {
        mOutputPoints.insert(point);
    }

    bool isOutputPoint(const std::string& point) const
    {
        return mOutputPoints.count(point) > 0;
    }

    void addBinaryTag(const std::string& tag)
    {
        mBinaryTags.push_back(tag);
        mBinarySet.insert(tag);
    }

    void addAnalogTag(const std::string& tag)
    {
        mAnalogTags.push_back(tag);
        mAnalogSet.insert(tag);
    }

    void addUpdatedBinaryTag(const std::string& tag, bool status)
    {
        std::scoped_lock<std::shared_mutex> lock(mBinaryMutex);
        mUpdatedBinaryTags[tag] = status;
    }

    void addUpdatedAnalogTag(const std::string& tag, double value)
    {
        std::scoped_lock<std::shared_mutex> lock(mAnalogMutex);
        mUpdatedAnalogTags[tag] = value;
    }

    const std::vector<std::string>& getBinaryTags() const
    {
        return mBinaryTags;
    }

    const std::vector<std::string>& getAnalogTags() const
    {
        return mAnalogTags;
    }

    const std::map<std::string, bool>& getUpdatedBinaryTags() const
    {
        return mUpdatedBinaryTags;
    }

    const std::map<std::string, double>& getUpdatedAnalogTags() const
    {
        return mUpdatedAnalogTags;
    }

    std::map<std::string, bool> snapshotUpdatedBinaryTags()
    {
        std::lock_guard<std::shared_mutex> lock(mBinaryMutex);
        return mUpdatedBinaryTags;
    }

    std::map<std::string, double> snapshotUpdatedAnalogTags()
    {
        std::lock_guard<std::shared_mutex> lock(mAnalogMutex);
        return mUpdatedAnalogTags;
    }

    bool isUpdatedBinaryTag(const std::string& tag)
    {
        std::scoped_lock<std::shared_mutex> lock(mBinaryMutex);
        return mUpdatedBinaryTags.count(tag);
    }

    bool isUpdatedAnalogTag(const std::string& tag)
    {
        std::scoped_lock<std::shared_mutex> lock(mAnalogMutex);
        return mUpdatedAnalogTags.count(tag);
    }

    // Apply queued internal-tag updates, then drop them (external tags stay for OutputModule).
    // Lock is held throughout, so the erased value is exactly the one applied.
    void updateInternalData()
    {
        {
            std::scoped_lock<std::shared_mutex> lock(mBinaryMutex);
            for (auto it = mUpdatedBinaryTags.begin(); it != mUpdatedBinaryTags.end();)
            {
                if (mInternalData->setData<bool>(it->first, it->second))
                {
                    it = mUpdatedBinaryTags.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        {
            std::scoped_lock<std::shared_mutex> lock(mAnalogMutex);
            for (auto it = mUpdatedAnalogTags.begin(); it != mUpdatedAnalogTags.end();)
            {
                if (mInternalData->setData<double>(it->first, it->second))
                {
                    it = mUpdatedAnalogTags.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    // erase only if the value is still the one that was written (a newer update stays queued)
    void deleteBinaryTag(const std::string& tag, bool expected)
    {
        std::lock_guard<std::shared_mutex> lock(mBinaryMutex);
        auto it = mUpdatedBinaryTags.find(tag);
        if (it != mUpdatedBinaryTags.end() && it->second == expected)
        {
            mUpdatedBinaryTags.erase(it);
        }
    }

    void deleteAnalogTag(const std::string& tag, double expected)
    {
        std::lock_guard<std::shared_mutex> lock(mAnalogMutex);
        auto it = mUpdatedAnalogTags.find(tag);
        if (it != mUpdatedAnalogTags.end() && it->second == expected)
        {
            mUpdatedAnalogTags.erase(it);
        }
    }

    void clearUpdatedTags()
    {
        {
            std::lock_guard<std::shared_mutex> lock(mBinaryMutex);
            mUpdatedBinaryTags.clear();
        }
        {
            std::lock_guard<std::shared_mutex> lock(mAnalogMutex);
            mUpdatedAnalogTags.clear();
        }
    }

    bool isBinary(const std::string& tag)
    {
        return mBinarySet.count(tag) != 0;
    }

    bool isAnalog(const std::string& tag)
    {
        return mAnalogSet.count(tag) != 0;
    }

private:
    std::shared_ptr<DataStore<std::string>> mInternalData; // internal tags
    std::shared_ptr<DataStore<std::string>> mExternalData; // i/o points
    std::map<std::string, std::string> mExternalPoints;    // id ==> point
    std::map<std::string, std::string> mTagToPoint;        // tag ==> point
    std::vector<std::string> mBinaryTags;
    std::vector<std::string> mAnalogTags;
    std::unordered_set<std::string> mBinarySet; // O(1) isBinary/isAnalog; vectors keep order
    std::unordered_set<std::string> mAnalogSet;
    std::unordered_set<std::string> mOutputPoints;
    std::map<std::string, bool> mUpdatedBinaryTags;
    std::map<std::string, double> mUpdatedAnalogTags;
    std::shared_mutex mBinaryMutex;
    std::shared_mutex mAnalogMutex;
};

} // namespace field_device
} // namespace bennu

#endif // BENNU_FIELDDEVICE_DATAMANAGER_HPP
