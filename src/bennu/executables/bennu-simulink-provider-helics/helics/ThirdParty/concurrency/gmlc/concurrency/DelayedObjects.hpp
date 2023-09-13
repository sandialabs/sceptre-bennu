/*
Copyright (c) 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once

#include <future>
#include <map>
#include <mutex>

namespace gmlc {
namespace concurrency {
    /** class holding a set of delayed objects, the delayed object are held by
 * promises*/
    template<class X>
    class DelayedObjects {
      private:
        std::map<int, std::promise<X>> promiseByInteger;
        std::map<std::string, std::promise<X>> promiseByString;
        std::mutex promiseLock;
        std::map<int, std::promise<X>> usedPromiseByInteger;
        std::map<std::string, std::promise<X>> usedPromiseByString;

      public:
        DelayedObjects() = default;
        /// On destruction set a default value for all object
        ~DelayedObjects()
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            for (auto& obj : promiseByInteger) {
                obj.second.set_value(X{});
            }
            for (auto& obj : promiseByString) {
                obj.second.set_value(X{});
            }
        }
        // not movable or copyable;
        DelayedObjects(const DelayedObjects&) = delete;
        DelayedObjects(DelayedObjects&&) = delete;
        DelayedObjects& operator=(const DelayedObjects&) = delete;
        DelayedObjects& operator=(DelayedObjects&&) = delete;

        /// set the value for delayed indexed object
        void setDelayedValue(int index, const X& val)
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            auto fnd = promiseByInteger.find(index);
            if (fnd != promiseByInteger.end()) {
                fnd->second.set_value(val);
                usedPromiseByInteger[index] = std::move(fnd->second);
                promiseByInteger.erase(fnd);
            }
        }
        /// set the value for delayed named object
        void setDelayedValue(const std::string& name, const X& val)
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            auto fnd = promiseByString.find(name);
            if (fnd != promiseByString.end()) {
                fnd->second.set_value(val);
                usedPromiseByString[name] = std::move(fnd->second);
                promiseByString.erase(fnd);
            }
        }
		/// check whether the index is known (either fulfilled or unfulfilled)
        bool isRecognized(int index)
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            auto fnd = promiseByInteger.find(index);
            if (fnd != promiseByInteger.end()) {
                return true;
            }
            auto fnd2 = usedPromiseByInteger.find(index);
            if (fnd2 != usedPromiseByInteger.end()) {
                return true;
            }
            return false;
        }
        /// check whether the name is known (either fulfilled or unfulfilled)
        bool isRecognized(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            auto fnd = promiseByString.find(name);
            if (fnd != promiseByString.end()) {
                return true;
            }
            auto fnd2 = usedPromiseByString.find(name);
            if (fnd2 != usedPromiseByString.end()) {
                return true;
            }
            return false;
        }

		/// check whether the index is known and completed
        bool isCompleted(int index)
		{
            std::lock_guard<std::mutex> lock(promiseLock);
            auto fnd = usedPromiseByInteger.find(index);
            return (fnd != usedPromiseByInteger.end());
		}
        /// check whether the string is known and completed
        bool isCompleted(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            auto fnd = usedPromiseByString.find(name);
            return (fnd != usedPromiseByString.end());
        }

        /// For all remaining promises set them to a value of val
        void fulfillAllPromises(const X& val)
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            for (auto& pr : promiseByInteger) {
                pr.second.set_value(val);
                usedPromiseByInteger[pr.first] = std::move(pr.second);
            }
            for (auto& pr : promiseByString) {
                pr.second.set_value(val);
                usedPromiseByString[pr.first] = std::move(pr.second);
            }
            promiseByInteger.clear();
            promiseByString.clear();
        }

        /// create a delayed object with an index
        std::future<X> getFuture(int index)
        {
            auto V = std::promise<X>();
            auto fut = V.get_future();
            std::lock_guard<std::mutex> lock(promiseLock);
            promiseByInteger[index] = std::move(V);
            return fut;
        }
        /// create a delayed object with a name
        std::future<X> getFuture(const std::string& name)
        {
            auto V = std::promise<X>();
            auto fut = V.get_future();
            std::lock_guard<std::mutex> lock(promiseLock);
            promiseByString[name] = std::move(V);
            return fut;
        }
        /// Indicate that the user is finished accessing a value by index and it can
        /// be deleted
        void finishedWithValue(int index)
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            auto fnd = usedPromiseByInteger.find(index);
            if (fnd != usedPromiseByInteger.end()) {
                usedPromiseByInteger.erase(fnd);
            }
        }
        /// Indicate that the user is finished accessing a value by string and it
        /// can be deleted
        void finishedWithValue(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(promiseLock);
            auto fnd = usedPromiseByString.find(name);
            if (fnd != usedPromiseByString.end()) {
                usedPromiseByString.erase(fnd);
            }
        }
    };

} // namespace concurrency
} // namespace gmlc
