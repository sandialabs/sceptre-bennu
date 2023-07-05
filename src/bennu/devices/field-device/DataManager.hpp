#ifndef BENNU_FIELDDEVICE_DATAMANAGER_HPP
#define BENNU_FIELDDEVICE_DATAMANAGER_HPP

#include <algorithm>
#include <iostream>
#include <map>
#include <mutex> // std::scoped_lock
#include <shared_mutex>
#include <string>
#include <vector>

#include "bennu/devices/field-device/DataStore.hpp"

namespace bennu {
namespace field_device {

class DataManager
{
public:
    DataManager() :
        mInternalData(new DataStore<std::string>), // tags
        mExternalData(new DataStore<std::string>) // i/o points
    {
    }

    ~DataManager() = default;

    // add Binary/Analog data point from Input/Output modules
    template<typename T>
    void addExternalData(const std::string& id, const std::string& point)
    {
        mExternalData->addData(point, T());
        mExternalPoints[id] = point;
    }

    // add 'internal-tag' data point
    template<typename T>
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

    template<typename T>
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

    template<typename T>
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

    template<typename T>
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

    void addBinaryTag(const std::string& tag)
    {
        mBinaryTags.push_back(tag);
    }

    void addAnalogTag(const std::string& tag)
    {
        mAnalogTags.push_back(tag);
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

    //  Update any internal-tag data values
    void updateInternalData()
    {
        {
            std::scoped_lock<std::shared_mutex> lock(mBinaryMutex);
            for (auto& t : mUpdatedBinaryTags)
            {
                if (mInternalData->hasData(t.first))
                {
                    mInternalData->setData<bool>(t.first, t.second);
                }
            }
        }
        {
            std::scoped_lock<std::shared_mutex> lock(mAnalogMutex);
            for (auto& t : mUpdatedAnalogTags)
            {
                if (mInternalData->hasData(t.first))
                {
                    mInternalData->setData<double>(t.first, t.second);
                }
            }
        }
    }

    void clearUpdatedTags()
    {
        {
            std::lock_guard<std::shared_mutex> lock(mBinaryMutex);
            mUpdatedAnalogTags.clear();
        }
        {
            std::lock_guard<std::shared_mutex> lock(mAnalogMutex);
            mUpdatedBinaryTags.clear();
        }
    }

    bool isBinary(const std::string& tag)
    {
        return std::find(mBinaryTags.begin(), mBinaryTags.end(), tag) != mBinaryTags.end();
    }

    bool isAnalog(const std::string& tag)
    {
        return std::find(mAnalogTags.begin(), mAnalogTags.end(), tag) != mAnalogTags.end();
    }


private:
    std::shared_ptr<DataStore<std::string>> mInternalData; // internal tags
    std::shared_ptr<DataStore<std::string>> mExternalData; // i/o points
    std::map<std::string, std::string> mExternalPoints; // id ==> point
    std::map<std::string, std::string> mTagToPoint; // tag ==> point
    std::vector<std::string> mBinaryTags;
    std::vector<std::string> mAnalogTags;
    std::map<std::string, bool> mUpdatedBinaryTags;
    std::map<std::string, double> mUpdatedAnalogTags;
    std::shared_mutex mBinaryMutex;
    std::shared_mutex mAnalogMutex;

};

} // namespace field_device
} // namespace bennu

#endif // BENNU_FIELDDEVICE_DATAMANAGER_HPP
