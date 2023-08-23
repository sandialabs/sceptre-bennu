/*
Copyright (c) 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace gmlc {
namespace concurrency {
    /** namespace for the global variable in tripwire*/

    class Latch {
      public:
        explicit Latch(int start): counter_{start} {}
        /** arrive at a synchronization point (non_blocking)*/
        void arrive()
        {
            std::unique_lock<std::mutex> lck(mtx);
            --counter_;
            if (counter_ == 0) {
                cv.notify_all();
            }
        }
        /** wait for the required number of threads to arrive and then proceed*/
        void wait()
        {
            if (counter_ > 0) {
                std::unique_lock<std::mutex> lck(mtx);
                while (counter_.load() > 0) {
                    cv.wait(lck);
                }
            }
        }
        /** arrive at a synchronization point and wait for everyone else*/
        void arrive_and_wait()
        {
            arrive();
            wait();
        }

      private:
        std::mutex mtx;
        std::condition_variable cv;
        std::atomic<int> counter_;
    };
} // namespace concurrency
} // namespace gmlc
