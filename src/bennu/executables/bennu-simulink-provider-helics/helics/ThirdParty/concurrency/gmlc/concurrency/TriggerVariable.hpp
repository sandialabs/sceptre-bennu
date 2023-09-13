/*
Copyright (c) 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace gmlc {
namespace concurrency {
    class TriggerVariable {
      public:
        explicit TriggerVariable(bool active = false): activated(active){};
        /** activate the trigger to the ready state
    @return true if the Trigger was activated false if it was already active
    */
        bool activate()
        {
            if (activated.load()) {
                return false;
            }
            {
                std::lock_guard<std::mutex> lock(triggerLock);
                triggered = false;
            }
            std::lock_guard<std::mutex> lock(activeLock);
            activated = true;
            cv_active.notify_all();
            return true;
        }
        /** trigger the variable
    @return true if the trigger was successful, false if the trigger has not
    been activated yet*/
        bool trigger()
        {
            if (!activated.load()) {
                return false;
            }
            std::lock_guard<std::mutex> lock(triggerLock);
            triggered.store(true);
            cv_trigger.notify_all();
            return true;
        }

        /** check if the variable has been triggered after the last activation*/
        bool isTriggered() const { return triggered.load(); }
        /** wait for the variable to trigger*/
        bool wait() const
        {
            if (!activated.load()) {
                return true;
            }
            std::unique_lock<std::mutex> lk(triggerLock);
            if (!triggered) {
                cv_trigger.wait(lk, [this] { return triggered.load(); });
            }
            return true;
        }
        /** wait for a period of time for the value to trigger*/
        bool wait_for(const std::chrono::milliseconds& duration) const
        {
            if (!isActive()) {
                return true;
            }
            std::unique_lock<std::mutex> lk(triggerLock);
            if (!triggered) {
                return cv_trigger.wait_for(lk, duration, [this] { return triggered.load(); });
            }
            return true;
        }
        /** wait on the Trigger becoming active*/
        void waitActivation() const
        {
            std::unique_lock<std::mutex> lk(activeLock);
            if (!activated) {
                cv_active.wait(lk, [this] { return activated.load(); });
            }
        }
        /** wait for a period of time for the value to trigger*/
        bool wait_forActivation(const std::chrono::milliseconds& duration) const
        {
            std::unique_lock<std::mutex> lk(activeLock);
            if (!activated) {
                return cv_active.wait_for(lk, duration, [this] { return activated.load(); });
            }
            return true;
        }
        /** reset the trigger Variable to the inactive state
    @details reset on an untriggered but active trigger variable will cause the
    trigger to occur and then be reset
    */
        void reset()
        {
            std::unique_lock<std::mutex> lk(activeLock);
            if (activated.load()) {
                while (!triggered.load(std::memory_order_acquire)) {
                    lk.unlock();
                    trigger();
                    lk.lock();
                }
                activated.store(false);
            }
        }
        /** check if the variable is active*/
        bool isActive() const { return activated.load(); }

      private:
        std::atomic_bool triggered{false}; //!< the state of the trigger
        mutable std::mutex triggerLock; //!< mutex protecting the trigger
        std::atomic_bool activated{
            false}; //!< variable controlling if the trigger has been activated
        mutable std::mutex activeLock; //!< mutex protecting the activation
        mutable std::condition_variable cv_trigger; //!< semaphore for the trigger
        mutable std::condition_variable cv_active; //!< semaphore for the activation
    };

} // namespace concurrency
} // namespace gmlc
