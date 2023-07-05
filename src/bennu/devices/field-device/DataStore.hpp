#ifndef BENNU_DEVICES_FIELDDEVICE_DATASTORE_HPP
#define BENNU_DEVICES_FIELDDEVICE_DATASTORE_HPP

#include <mutex> // std::scoped_lock
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>

namespace bennu {
namespace field_device {

template<typename P>
class DataStore
{
public:
    DataStore() {}

    ~DataStore()
    {
        clear();
    }

    void clear()
    {
        std::scoped_lock<std::shared_mutex> lock(mMutex);
        mData.clear();
    }

    const std::unordered_map<P, std::variant<int, double, bool>>& get() const
    {
        return mData;
    }

    template<typename T>
    void addData(const P& point, const T& value)
    {
        std::scoped_lock<std::shared_mutex> lock(mMutex);
        mData[point] = value;
        mTimestamps[point] = 0;
    }

    template<typename T>
    bool setData(const P& point, const T& value, const double ts = 0)
    {
        std::scoped_lock<std::shared_mutex> lock(mMutex);
        auto iter = mData.find(point);
        if (iter != mData.end())
        {
            iter->second = value;

            auto i = mTimestamps.find(point);
            if (i != mTimestamps.end())
            {
                i->second = ts;
            }

            return true;
        }
        return false;
    }


    template<typename T>
    T getData(const P& point)
    {
        std::scoped_lock<std::shared_mutex> lock(mMutex);
        auto iter = mData.find(point);
        if (iter != mData.end())
        {
            return std::get<T>(iter->second);
        }
        return T();
    }

    double getTimestamp(const P& device)
    {
        std::scoped_lock<std::shared_mutex> lock(mMutex);

        auto iter = mTimestamps.find(device);
        if (iter != mTimestamps.end())
        {
            return iter->second;
        }

        return 0;
    }

    bool hasData(const P& point)
    {
        std::scoped_lock<std::shared_mutex> lock(mMutex);
        return mData.find(point) != mData.end();
    }

    void printData()
    {
        if (mData.size()) { printf("\n============ DATA ============\n"); }
        std::scoped_lock<std::shared_mutex> lock(mMutex);
        for (const auto& kv : mData)
        {
            std::string val;
            std::string type;
            if (std::holds_alternative<int>(kv.second)) { val = std::to_string(std::get<int>(kv.second)); type = "int"; }
            else if (std::holds_alternative<double>(kv.second)) { val = std::to_string(std::get<double>(kv.second)); type = "double"; }
            else if (std::holds_alternative<bool>(kv.second)) { val = std::to_string(std::get<bool>(kv.second)); type = "bool"; }
            printf("%s -- %s\n", kv.first.data(), val.data());
        }
        if (mData.size()) { printf("==============================\n\n"); }
    }

private:
    std::shared_mutex mMutex;
    std::unordered_map<P, std::variant<int, double, bool>> mData;
    std::unordered_map<P, double> mTimestamps;
};

} // namespace field_device
} // namespace bennu

#endif // BENNU_DEVICES_FIELDDEVICE_DATASTORE_HPP
